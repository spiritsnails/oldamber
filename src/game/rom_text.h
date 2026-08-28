#pragma once
#include <stddef.h>

const char *RomText(const char *symbol);

const char *RomTextPrefixed(const char *prefix, const char *symbol);

const char *RomTextPage(const char *symbol, int page_index);

void RomTextSplice(char *dst, size_t dst_size, const char *symbol,
                   const char *token, const char *value);

void RomTextSpliceN(char *dst, size_t dst_size, const char *symbol, ...);

const char *PortText(const char *literal);
