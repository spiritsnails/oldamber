#include "update.h"
#include "app_version.h"
#include "data_dir.h"
#include "sha256.h"

#include <SDL.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define update_mkdir(path) _mkdir(path)
#else
#include <unistd.h>
#include <sys/wait.h>
#define update_mkdir(path) mkdir((path), 0755)
#endif

#define UPDATE_MANIFEST "https://github.com/spiritsnails/oldamber/releases/latest/download/oldamber-update.txt"

typedef struct {
    SDL_mutex *lock;
    SDL_Thread *thread;
    update_state_t state;
    char data[1024];
    char version[32];
    char url[1024];
    char sha256[65];
    uint64_t size;
    char message[160];
    int test_manifest;
} update_ctx_t;

static update_ctx_t u;

static int exists(const char *path) { struct stat st; return stat(path, &st) == 0; }

static int make_tree(const char *path) {
    char copy[1200]; size_t n = strlen(path);
    if (!n || n >= sizeof copy) return 0;
    memcpy(copy,path,n+1);
    for (char *p=copy+1;*p;++p) {
        if (*p!='/' && *p!='\\') continue;
        char c=*p; *p='\0';
        if (!exists(copy) && update_mkdir(copy)!=0 && errno!=EEXIST) return 0;
        *p=c;
    }
    return exists(copy) || update_mkdir(copy)==0 || errno==EEXIST;
}

static void set_state(update_state_t state, const char *message) {
    SDL_LockMutex(u.lock);
    u.state=state;
    snprintf(u.message,sizeof u.message,"%s",message ? message : "");
    SDL_UnlockMutex(u.lock);
    if (message && message[0]) fprintf(stderr,"[update] %s\n",message);
}

static int run_process(const char *const *args) {
#ifdef _WIN32
    size_t cap=1;
    for (int i=0;args[i];++i) cap += strlen(args[i])*2+4;
    char *cmd=(char *)calloc(cap,1);
    if (!cmd) return 0;
    for (int i=0;args[i];++i) {
        if (i) strcat(cmd," ");
        strcat(cmd,"\"");
        for (const char *p=args[i];*p;++p) {
            if (*p=='\"') strcat(cmd,"\\");
            size_t n=strlen(cmd); cmd[n]=*p; cmd[n+1]='\0';
        }
        strcat(cmd,"\"");
    }
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si,0,sizeof si); memset(&pi,0,sizeof pi); si.cb=sizeof si;
    int ok=CreateProcessA(NULL,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)!=0;
    free(cmd); if (!ok) return 0;
    WaitForSingleObject(pi.hProcess,INFINITE);
    DWORD code=1; GetExitCodeProcess(pi.hProcess,&code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return code==0;
#else
    pid_t pid=fork();
    if (pid==0) { execvp(args[0],(char *const *)args); _exit(127); }
    if (pid<0) return 0;
    int status; if (waitpid(pid,&status,0)<0) return 0;
    return WIFEXITED(status) && WEXITSTATUS(status)==0;
#endif
}

static int download(const char *url, const char *path, int short_timeout) {
#ifdef _WIN32
    const char *curl="curl.exe";
#else
    const char *curl="curl";
#endif
    const char *check_args[]={curl,"-fL","--retry","2","--connect-timeout","10","--max-time","30","-o",path,url,NULL};
    const char *get_args[]={curl,"-fL","--retry","2","--connect-timeout","10","-o",path,url,NULL};
    return run_process(short_timeout ? check_args : get_args);
}

static int parse_semver(const char *s, unsigned *a, unsigned *b, unsigned *c) {
    char tail;
    return sscanf(s,"%u.%u.%u%c",a,b,c,&tail)==3;
}

static int version_newer(const char *candidate) {
    unsigned a,b,c,x,y,z;
    if (!parse_semver(candidate,&a,&b,&c) || !parse_semver(OLDAMBER_VERSION,&x,&y,&z)) return 0;
    if (a!=x) return a>x;
    if (b!=y) return b>y;
    return c>z;
}

static int valid_sha(const char *s) {
    if (strlen(s)!=64) return 0;
    for (int i=0;i<64;++i) if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

static int valid_url(const char *s) {
    static const char prefix[] =
        "https://github.com/spiritsnails/oldamber/releases/download/";
    if (u.test_manifest) return !strncmp(s,"https://",8) || !strncmp(s,"file://",7);
    return !strncmp(s, prefix, sizeof prefix - 1);
}

static int version_dir_name(const char *version, char *out, size_t cap) {
    size_t n = strlen(version);
    if (n + 2 > cap) return 0;
    out[0] = 'v';
    for (size_t i = 0; i < n; i++)
        out[i + 1] = version[i] == '.' ? '_' : version[i];
    out[n + 1] = '\0';
    return 1;
}

static const char *platform_key(void) {
#ifdef _WIN32
    return "windows-x64";
#elif defined(__APPLE__)
    return "macos-universal";
#else
    return "linux-x64";
#endif
}

static void trim(char *s) {
    char *start=s; while (*start && isspace((unsigned char)*start)) ++start;
    if (start!=s) memmove(s,start,strlen(start)+1);
    size_t n=strlen(s); while (n && isspace((unsigned char)s[n-1])) s[--n]='\0';
}

static int parse_manifest(const char *path) {
    FILE *f=fopen(path,"rb"); if (!f) return 0;
    char line[1400], version[32]="", url[1024]="", sha[65]="";
    unsigned schema=0, protocol=0; uint64_t size=0;
    char prefix[64]; snprintf(prefix,sizeof prefix,"%s.",platform_key());
    while (fgets(line,sizeof line,f)) {
        trim(line); if (!line[0] || line[0]=='#') continue;
        char *eq=strchr(line,'='); if (!eq) continue; *eq++='\0'; trim(line); trim(eq);
        if (!strcmp(line,"schema")) schema=(unsigned)strtoul(eq,NULL,10);
        else if (!strcmp(line,"version")) snprintf(version,sizeof version,"%s",eq);
        else if (!strcmp(line,"minimum-bootstrap")) protocol=(unsigned)strtoul(eq,NULL,10);
        else if (!strncmp(line,prefix,strlen(prefix))) {
            const char *field=line+strlen(prefix);
            if (!strcmp(field,"url")) snprintf(url,sizeof url,"%s",eq);
            else if (!strcmp(field,"sha256")) snprintf(sha,sizeof sha,"%s",eq);
            else if (!strcmp(field,"size")) size=(uint64_t)strtoull(eq,NULL,10);
        }
    }
    fclose(f);
    if (!version_newer(version)) {
        unsigned a,b,c;
        if (parse_semver(version,&a,&b,&c)) set_state(UPDATE_CURRENT,"GAME IS UP TO DATE");
        else set_state(UPDATE_ERROR,"UPDATE INFORMATION IS INVALID");
        return 0;
    }
    if (schema!=1 || protocol==0) { set_state(UPDATE_ERROR,"UPDATE FORMAT IS NOT SUPPORTED"); return 0; }
    if (protocol>OLDAMBER_UPDATE_PROTOCOL) { set_state(UPDATE_ERROR,"A NEW INSTALLER IS REQUIRED"); return 0; }
    if (!valid_url(url) || !valid_sha(sha) || size==0) { set_state(UPDATE_ERROR,"UPDATE INFORMATION IS INCOMPLETE"); return 0; }
    SDL_LockMutex(u.lock);
    snprintf(u.version,sizeof u.version,"%s",version);
    snprintf(u.url,sizeof u.url,"%s",url);
    for (int i=0;i<64;++i) u.sha256[i]=(char)tolower((unsigned char)sha[i]);
    u.sha256[64]='\0';
    u.size=size; u.state=UPDATE_AVAILABLE;
    snprintf(u.message,sizeof u.message,"VERSION %s IS AVAILABLE",version);
    SDL_UnlockMutex(u.lock);
    fprintf(stderr,"[update] version %s is available for %s\n",version,platform_key());
    return 1;
}

static int write_pointer(const char *name, const char *value) {
    char path[1200], tmp[1200];
    snprintf(path,sizeof path,"%s/%s",u.data,name);
    snprintf(tmp,sizeof tmp,"%s.new",path);
    FILE *f=fopen(tmp,"wb"); if (!f) return 0;
    int ok=fprintf(f,"%s\n",value)>0;
    if (fclose(f)!=0) ok=0;
    if (!ok) { remove(tmp); return 0; }
#ifdef _WIN32
    remove(path);
#endif
    if (rename(tmp,path)!=0) { remove(tmp); return 0; }
    return 1;
}

static void mark_healthy(void) {
    char path[1200], pending[32];
    const char *running = SDL_getenv("OLDAMBER_RUNNING_VERSION");
    if (!running || !running[0]) running = OLDAMBER_VERSION;
    snprintf(path,sizeof path,"%s/pending-version",u.data);
    FILE *f=fopen(path,"rb"); if (!f) return;
    if (!fgets(pending,sizeof pending,f)) pending[0]='\0';
    fclose(f); pending[strcspn(pending,"\r\n")]='\0';
    if (!strcmp(pending,running)) remove(path);
}

static int install_download(void) {
    char updates[1200], versions[1200], archive[1200], staging[1200], final[1200], child[1200], digest[65];
    char version_dir[sizeof u.version + 2];
    if (!version_dir_name(u.version, version_dir, sizeof version_dir)) return 0;
    snprintf(updates,sizeof updates,"%s/updates",u.data);
    snprintf(versions,sizeof versions,"%s/versions",u.data);
    if (!make_tree(updates) || !make_tree(versions)) return 0;
    snprintf(archive,sizeof archive,"%s/%s.tar.gz.part",updates,u.version);
    remove(archive);
    if (!download(u.url,archive,0)) { set_state(UPDATE_ERROR,"UPDATE DOWNLOAD FAILED"); return 0; }
    struct stat st;
    if (stat(archive,&st)!=0 || (uint64_t)st.st_size!=u.size) { set_state(UPDATE_ERROR,"UPDATE DOWNLOAD WAS INCOMPLETE"); return 0; }
    if (!Sha256_FileHex(archive,digest) || strcmp(digest,u.sha256)!=0) { set_state(UPDATE_ERROR,"UPDATE VERIFICATION FAILED"); return 0; }
    snprintf(final,sizeof final,"%s/%s",versions,version_dir);
    if (!exists(final)) {
        snprintf(staging,sizeof staging,"%s/.%s-%u",versions,version_dir,(unsigned)SDL_GetTicks());
        if (!make_tree(staging)) { set_state(UPDATE_ERROR,"COULD NOT STAGE UPDATE"); return 0; }
#ifdef _WIN32
        const char *tar="tar.exe";
#else
        const char *tar="tar";
#endif
        const char *args[]={tar,"-xzf",archive,"-C",staging,NULL};
        if (!run_process(args)) { set_state(UPDATE_ERROR,"COULD NOT UNPACK UPDATE"); return 0; }
#ifdef _WIN32
        snprintf(child,sizeof child,"%s/oldamber-game.exe",staging);
#else
        snprintf(child,sizeof child,"%s/oldamber-game",staging);
#endif
        if (!exists(child) || rename(staging,final)!=0) { set_state(UPDATE_ERROR,"UPDATE PAYLOAD IS INVALID"); return 0; }
    }
#ifdef _WIN32
    snprintf(child,sizeof child,"%s/oldamber-game.exe",final);
#else
    snprintf(child,sizeof child,"%s/oldamber-game",final);
#endif
    if (!exists(child)) { set_state(UPDATE_ERROR,"INSTALLED UPDATE IS INCOMPLETE"); return 0; }
    if (!write_pointer("previous-version",OLDAMBER_VERSION)) { set_state(UPDATE_ERROR,"COULD NOT PREPARE ROLLBACK"); return 0; }
    if (!write_pointer("pending-version",u.version)) { set_state(UPDATE_ERROR,"COULD NOT PREPARE UPDATE"); return 0; }
    if (!write_pointer("current-version",u.version)) { set_state(UPDATE_ERROR,"COULD NOT ACTIVATE UPDATE"); return 0; }
    remove(archive);
    set_state(UPDATE_READY,"UPDATE READY - RESTART TO APPLY");
    return 1;
}

static int worker(void *mode_ptr) {
    int mode=(int)(intptr_t)mode_ptr;
    if (mode==1) {
        char dir[1200], path[1200]; const char *override=SDL_getenv("OLDAMBER_UPDATE_MANIFEST");
        snprintf(dir,sizeof dir,"%s/updates",u.data);
        if (!make_tree(dir)) { set_state(UPDATE_ERROR,"COULD NOT OPEN UPDATE STORAGE"); return 0; }
        snprintf(path,sizeof path,"%s/latest.txt",dir);
        const char *url=(override&&override[0])?override:UPDATE_MANIFEST;
        if (!download(url,path,1)) { set_state(UPDATE_ERROR,"COULD NOT CHECK FOR UPDATES"); return 0; }
        parse_manifest(path);
    } else install_download();
    return 0;
}

static void reap(void) {
    if (!u.thread) return;
    update_state_t state;
    SDL_LockMutex(u.lock); state=u.state; SDL_UnlockMutex(u.lock);
    if (state!=UPDATE_CHECKING && state!=UPDATE_DOWNLOADING) {
        SDL_WaitThread(u.thread,NULL); u.thread=NULL;
    }
}

void Update_Init(void) {
    memset(&u,0,sizeof u); u.state=UPDATE_DISABLED;
    if (!SDL_getenv("OLDAMBER_BOOTSTRAPPED") || SDL_getenv("FLATPAK_ID") || SDL_getenv("OLDAMBER_DISABLE_UPDATES")) return;
    if (!UserDataDir_Get(u.data,sizeof u.data)) return;
    u.lock=SDL_CreateMutex(); if (!u.lock) return;
    u.state=UPDATE_IDLE;
    u.test_manifest=SDL_getenv("OLDAMBER_UPDATE_MANIFEST")!=NULL;
    mark_healthy();
    Update_Check();
}

void Update_Shutdown(void) {
    if (!u.lock) return;
    if (u.thread) { SDL_WaitThread(u.thread,NULL); u.thread=NULL; }
    SDL_DestroyMutex(u.lock); u.lock=NULL;
}

void Update_GetSnapshot(update_snapshot_t *out) {
    memset(out,0,sizeof *out);
    if (!u.lock) { out->state=UPDATE_DISABLED; return; }
    reap(); SDL_LockMutex(u.lock);
    out->state=u.state; snprintf(out->version,sizeof out->version,"%s",u.version);
    snprintf(out->message,sizeof out->message,"%s",u.message);
    SDL_UnlockMutex(u.lock);
}

void Update_Check(void) {
    if (!u.lock) return;
    reap();
    if (u.thread) return;
    set_state(UPDATE_CHECKING,"CHECKING FOR UPDATES...");
    u.thread=SDL_CreateThread(worker,"oldamber-update-check",(void *)(intptr_t)1);
    if (!u.thread) set_state(UPDATE_ERROR,"COULD NOT START UPDATE CHECK");
}

void Update_Install(void) {
    if (!u.lock) return;
    reap();
    if (u.thread || u.state!=UPDATE_AVAILABLE) return;
    set_state(UPDATE_DOWNLOADING,"DOWNLOADING AND VERIFYING UPDATE...");
    u.thread=SDL_CreateThread(worker,"oldamber-update-install",(void *)(intptr_t)2);
    if (!u.thread) set_state(UPDATE_ERROR,"COULD NOT START UPDATE DOWNLOAD");
}
