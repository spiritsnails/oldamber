
#include "rom_import.h"

#include <Python.h>
#include <stdio.h>
#include <string.h>

static const unsigned char kNintendoLogo[48] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
    0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63,
    0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

int RomImport_LooksLikeGBRom(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    int ok = 0;
    if (fseek(f, 0, SEEK_END) == 0) {
        long size = ftell(f);

        if (size >= 0x8000 && (size % 0x8000) == 0) {
            unsigned char logo[48];
            if (fseek(f, 0x104, SEEK_SET) == 0 &&
                fread(logo, 1, sizeof(logo), f) == sizeof(logo) &&
                memcmp(logo, kNintendoLogo, sizeof(logo)) == 0) {
                ok = 1;
            }
        }
    }
    fclose(f);
    return ok;
}

static void capture_py_error(char *err, size_t errsz) {
    if (!PyErr_Occurred()) {
        snprintf(err, errsz, "unknown extraction failure");
        return;
    }
    if (PyErr_ExceptionMatches(PyExc_SystemExit)) {
        PyObject *type, *value, *tb;
        PyErr_Fetch(&type, &value, &tb);
        PyErr_NormalizeException(&type, &value, &tb);
        PyObject *code = value ? PyObject_GetAttrString(value, "code") : NULL;
        if (code && PyUnicode_Check(code)) {
            snprintf(err, errsz, "%s", PyUnicode_AsUTF8(code));
        } else if (code && PyLong_Check(code) && PyLong_AsLong(code) == 0) {
            if (errsz) err[0] = '\0';
        } else {
            snprintf(err, errsz, "extraction stopped (see pokered_log.txt)");
        }
        Py_XDECREF(code);
        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(tb);
    } else {

        PyErr_Print();
        snprintf(err, errsz,
                "extraction failed with a Python error (see pokered_log.txt)");
    }
    PyErr_Clear();
}

static void ensure_python_initialized(void) {
    if (!Py_IsInitialized()) {
        Py_SetProgramName(L"pokered");

        Py_Initialize();
    }
}

static int add_to_sys_path(const char *dir, char *err, size_t errsz) {
    PyObject *sys_path = PySys_GetObject("path");
    if (!sys_path) {
        snprintf(err, errsz, "embedded Python: sys.path missing");
        return 0;
    }
    PyObject *dir_obj = PyUnicode_DecodeFSDefault(dir);
    if (!dir_obj || PyList_Insert(sys_path, 0, dir_obj) != 0) {
        Py_XDECREF(dir_obj);
        snprintf(err, errsz, "embedded Python: could not add '%s' to sys.path", dir);
        PyErr_Clear();
        return 0;
    }
    Py_DECREF(dir_obj);
    return 1;
}

static int run_module_main(const char *module_name, const char *const *argv_strs,
                           int argc, char *err, size_t errsz) {
    wchar_t **argv = PyMem_RawMalloc(sizeof(wchar_t *) * (size_t)argc);
    if (!argv) {
        snprintf(err, errsz, "embedded Python: out of memory");
        return 0;
    }
    for (int i = 0; i < argc; i++) {
        argv[i] = Py_DecodeLocale(argv_strs[i], NULL);
        if (!argv[i]) {
            snprintf(err, errsz, "embedded Python: bad path encoding");
            for (int j = 0; j < i; j++) PyMem_RawFree(argv[j]);
            PyMem_RawFree(argv);
            return 0;
        }
    }
    PySys_SetArgv(argc, argv);
    for (int i = 0; i < argc; i++) PyMem_RawFree(argv[i]);
    PyMem_RawFree(argv);

    PyObject *module = PyImport_ImportModule(module_name);
    if (!module) {
        capture_py_error(err, errsz);
        return 0;
    }

    PyObject *main_fn = PyObject_GetAttrString(module, "main");
    Py_DECREF(module);
    if (!main_fn || !PyCallable_Check(main_fn)) {
        Py_XDECREF(main_fn);
        snprintf(err, errsz, "%s.py has no callable main()", module_name);
        return 0;
    }

    PyObject *result = PyObject_CallObject(main_fn, NULL);
    Py_DECREF(main_fn);

    if (!result) {
        capture_py_error(err, errsz);

        return err[0] == '\0';
    }

    long code = PyLong_Check(result) ? PyLong_AsLong(result) : 0;
    Py_DECREF(result);
    if (code != 0) {
        snprintf(err, errsz, "%s.py exited with status %ld", module_name, code);
        return 0;
    }
    return 1;
}

int RomImport_BuildPak(const char *rom_path, const char *tools_dir,
                       const char *out_pak_path, char *err, size_t errsz) {
    if (err && errsz) err[0] = '\0';
    ensure_python_initialized();
    if (!add_to_sys_path(tools_dir, err, errsz)) return 0;

    const char *argv[] = { "build_pak.py", "--rom", rom_path, "--out", out_pak_path };
    return run_module_main("build_pak", argv, (int)(sizeof(argv) / sizeof(argv[0])),
                           err, errsz);
}

int RomImport_EmitKantoMaps(const char *rom_path, const char *romimport_tools_dir,
                            char *err, size_t errsz) {
    if (err && errsz) err[0] = '\0';
    ensure_python_initialized();
    if (!add_to_sys_path(romimport_tools_dir, err, errsz)) return 0;

    const char *art_argv[] = { "emit_kanto.py", "--rom", rom_path, "--art-all" };
    if (!run_module_main("emit_kanto", art_argv,
                         (int)(sizeof(art_argv) / sizeof(art_argv[0])), err, errsz))
        return 0;

    const char *all_argv[] = { "emit_kanto.py", "--rom", rom_path, "--all" };
    if (!run_module_main("emit_kanto", all_argv,
                         (int)(sizeof(all_argv) / sizeof(all_argv[0])), err, errsz))
        return 0;

    const char *text_argv[] = { "emit_scene_text.py", "--rom", rom_path };
    return run_module_main("emit_scene_text", text_argv,
                           (int)(sizeof(text_argv) / sizeof(text_argv[0])), err, errsz);
}
