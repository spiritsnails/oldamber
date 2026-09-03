#include "launcher_save_editor_internal.h"
#include "launcher_dropdown.h"
#include "launcher_save_editor_location.h"
#include "hardware.h"
#include "data_dir.h"
#include "../data/base_stats.h"
#include "../data/event_flag_ids.h"
#include "../data/moves_data.h"
#include "../data/pokemon_names_gen.h"
#include "../game/pokemon.h"
#include "../game/overworld.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WDD_NONE, WDD_SPECIES, WDD_STATUS, WDD_MOVE, WDD_PARTY_SLOT, WDD_BOX, WDD_BOX_SLOT, WDD_LOCATION };
static launcher_dropdown_t workspace_dropdown;
static int workspace_dropdown_kind;
static int workspace_dropdown_aux;
static box_mon_t *workspace_dropdown_mon;

typedef void (*inline_commit_fn)(save_editor_t *, void *, void *, int,
                                 uint32_t, const char *);
typedef struct {
    int open;
    int name;
    int replace_on_type;
    SDL_Rect field;
    char buffer[64];
    uint32_t minimum;
    uint32_t maximum;
    inline_commit_fn commit;
    void *ctx;
    void *ctx2;
    int aux;
} inline_edit_t;
static inline_edit_t inline_edit;

static int hit(save_editor_t *e, SDL_Rect r) {
    return LauncherDraw_PointInRect(e->nav->ptr_x, e->nav->ptr_y, r);
}

static int dropdown_owns_rect(SDL_Rect r) {
    SDL_Rect f = workspace_dropdown.field;
    return workspace_dropdown.open && r.x == f.x && r.y == f.y &&
           r.w == f.w && r.h == f.h;
}

static int inline_owns_rect(SDL_Rect r) {
    SDL_Rect f = inline_edit.field;
    return inline_edit.open && r.x == f.x && r.y == f.y &&
           r.w == f.w && r.h == f.h;
}

static void finish_inline_edit(save_editor_t *e, int commit) {
    if (!inline_edit.open) return;
    if (commit && inline_edit.commit) {
        uint32_t value = 0;
        if (!inline_edit.name) {
            value = (uint32_t)strtoul(inline_edit.buffer[0] ? inline_edit.buffer : "0", NULL, 10);
            if (value < inline_edit.minimum) value = inline_edit.minimum;
            if (value > inline_edit.maximum) value = inline_edit.maximum;
        }
        inline_edit.commit(e,inline_edit.ctx,inline_edit.ctx2,inline_edit.aux,
                           value,inline_edit.buffer);
    }
    inline_edit.open = 0;
    SDL_StopTextInput();
}

static void begin_inline_number(save_editor_t *e, SDL_Rect field,
                                const char *title, uint32_t current,
                                uint32_t minimum, uint32_t maximum,
                                inline_commit_fn commit, void *ctx,
                                void *ctx2, int aux) {
    if (LauncherNav_Device(e->nav) == LNAV_INPUT_GAMEPAD) {
        uint32_t value;
        if (SE_EditNumber(e,title,current,minimum,maximum,&value))
            commit(e,ctx,ctx2,aux,value,NULL);
        return;
    }
    memset(&inline_edit,0,sizeof(inline_edit));
    inline_edit.open=1;inline_edit.replace_on_type=1;inline_edit.field=field;
    inline_edit.minimum=minimum;inline_edit.maximum=maximum;
    inline_edit.commit=commit;inline_edit.ctx=ctx;inline_edit.ctx2=ctx2;inline_edit.aux=aux;
    snprintf(inline_edit.buffer,sizeof(inline_edit.buffer),"%u",current);
    SDL_StartTextInput();
}

static void begin_inline_name(save_editor_t *e, SDL_Rect field,
                              const char *title, uint8_t *encoded,
                              inline_commit_fn commit) {
    if (LauncherNav_Device(e->nav) == LNAV_INPUT_GAMEPAD) {
        if (SE_EditText(e,title,encoded)) e->dirty=1;
        return;
    }
    memset(&inline_edit,0,sizeof(inline_edit));
    inline_edit.open=1;inline_edit.name=1;inline_edit.replace_on_type=1;
    inline_edit.field=field;inline_edit.commit=commit;inline_edit.ctx=encoded;
    SE_DecodeName(encoded,inline_edit.buffer,sizeof(inline_edit.buffer));
    SDL_StartTextInput();
}

static void begin_inline_ascii(SDL_Rect field, char *text,
                               inline_commit_fn commit) {
    memset(&inline_edit,0,sizeof(inline_edit));
    inline_edit.open=1;inline_edit.name=2;inline_edit.field=field;
    inline_edit.commit=commit;inline_edit.ctx=text;
    snprintf(inline_edit.buffer,sizeof(inline_edit.buffer),"%s",text);
    SDL_StartTextInput();
}

static void tick_inline_edit(save_editor_t *e, unsigned *in) {
    if (!inline_edit.open) return;
    *in = SE_FilterTextNavigation(*in, e->dropdown_backspace);
    if (e->dropdown_backspace) {
        size_t n=strlen(inline_edit.buffer);if(n)inline_edit.buffer[n-1]='\0';
        inline_edit.replace_on_type=0;*in&=~(LNAV_BACK|LNAV_CANCEL);
    }
    if (e->dropdown_text[0]) {
        if(inline_edit.replace_on_type){inline_edit.buffer[0]='\0';inline_edit.replace_on_type=0;}
        size_t n=strlen(inline_edit.buffer);
        for(const char*p=e->dropdown_text;*p&&n+1<sizeof(inline_edit.buffer);p++){
            int allowed=inline_edit.name==2?((unsigned char)*p>=32&&(unsigned char)*p<127):
                inline_edit.name?((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p==' '):
                (*p>='0'&&*p<='9');
            if(allowed&&(inline_edit.name!=1||n<NAME_LENGTH-1))inline_edit.buffer[n++]=*p;
        }
        inline_edit.buffer[n]='\0';
    }
    if(*in&LNAV_ACCEPT){finish_inline_edit(e,1);*in&=~LNAV_ACCEPT;return;}
    if(*in&LNAV_CANCEL){finish_inline_edit(e,0);*in&=~LNAV_CANCEL;return;}
    if(e->nav->ptr_pressed&&!hit(e,inline_edit.field)){finish_inline_edit(e,1);return;}
    *in=0;e->nav->ptr_pressed=0;e->nav->ptr_released=0;
}

static int control(save_editor_t *e, unsigned in, int id, SDL_Rect r) {
    if (e->nav->ptr_pressed && hit(e, r)) {
        if (e->focus != id) { e->focus = id; return 0; }
        return 1;
    }
    return e->focus == id && (in & LNAV_ACCEPT);
}

static void open_workspace_dropdown(save_editor_t *e, int kind, int aux,
                                    int control_id, SDL_Rect field, int count,
                                    int current, launcher_dropdown_label_fn label,
                                    void *ctx, box_mon_t *mon) {
    workspace_dropdown_kind = kind;
    workspace_dropdown_aux = aux;
    (void)control_id;
    workspace_dropdown_mon = mon;
    LauncherDropdown_Open(&workspace_dropdown, field, count, current,
                          e->nav->ptr_pressed != 0,
                          LauncherNav_Device(e->nav) == LNAV_INPUT_POINTER,
                          label, ctx);
}

static void button(save_editor_t *e, SDL_Rect r, const char *text, int focused) {
    LauncherDraw_Bevel(e->r, r, 1);
    if (focused) { SDL_Rect f = {r.x+3,r.y+3,r.w-6,r.h-6}; LauncherDraw_FocusBar(e->r,f); }
    Uint8 c = focused ? 0xFF : 0x00;
    LauncherDraw_TextBold(e->r, r.x+(r.w-LauncherDraw_TextWidthBold(1,text))/2,
                          LDRAW_TEXT_Y(r.y,r.h,1),1,c,c,c,text);
}

static void field(save_editor_t *e, int y, const char *label, const char *value,
                  int focused, int combo) {
    int lx=42, fx=210; SDL_Rect r={fx,y,300,24};
    char editing[72];
    if(inline_owns_rect(r)){snprintf(editing,sizeof(editing),"%s_",inline_edit.buffer);value=editing;focused=1;}
    LauncherDraw_TextBold(e->r,lx,LDRAW_TEXT_Y(y,24,1),1,LCOL_TEXT,label);
    if(combo){LauncherDropdown_DrawField(e->r,r,value,focused,dropdown_owns_rect(r));return;}
    LauncherDraw_Bevel(e->r,r,0);
    if(focused){SDL_Rect f={r.x+3,r.y+3,r.w-6,r.h-6};LauncherDraw_FocusBar(e->r,f);}
    Uint8 c=focused?0xFF:0x00;
    LauncherDraw_TextClippedBold(e->r,r.x+6,LDRAW_TEXT_Y(r.y,r.h,1),1,c,c,c,value,r.w-(combo?34:12));
}

static SDL_Rect compact_field(save_editor_t *e, int x, int y, int label_w,
                              int field_w, const char *label,
                              const char *value, int focused, int combo) {
    SDL_Rect r = { x + label_w, y, field_w, 21 };
    char editing[72];
    if(inline_owns_rect(r)){snprintf(editing,sizeof(editing),"%s_",inline_edit.buffer);value=editing;focused=1;}
    LauncherDraw_TextBold(e->r, x, LDRAW_TEXT_Y(y, 21, 1), 1, LCOL_TEXT, label);
    if (combo) {
        LauncherDropdown_DrawField(e->r, r, value, focused,
            dropdown_owns_rect(r));
        return r;
    }
    LauncherDraw_Bevel(e->r, r, 0);
    if (focused) {
        SDL_Rect f = { r.x + 3, r.y + 3, r.w - 6, r.h - 6 };
        LauncherDraw_FocusBar(e->r, f);
    }
    Uint8 c = focused ? 0xFF : 0x00;
    LauncherDraw_TextClippedBold(e->r, r.x + 6, LDRAW_TEXT_Y(r.y, r.h, 1),
                                 1, c, c, c, value, r.w - (combo ? 30 : 12));
    return r;
}

static void checkbox(save_editor_t *e, SDL_Rect r, const char *label, int checked, int focused) {
    SDL_Rect b={r.x,r.y+3,16,16};LauncherDraw_Bevel(e->r,b,0);
    if(checked)LauncherDraw_TextBold(e->r,b.x+4,b.y+3,1,LCOL_TEXT,"X");
    if(focused){SDL_Rect f={r.x+20,r.y,r.w-20,r.h};LauncherDraw_FocusBar(e->r,f);LauncherDraw_TextBold(e->r,r.x+24,LDRAW_TEXT_Y(r.y,r.h,1),1,LCOL_FOCUS_TXT,label);}
    else LauncherDraw_TextBold(e->r,r.x+24,LDRAW_TEXT_Y(r.y,r.h,1),1,LCOL_TEXT,label);
}

static int ci_contains(const char *s,const char *q){if(!q||!*q)return 1;for(;*s;s++){const char*a=s,*b=q;while(*a&&*b&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}if(!*b)return 1;}return 0;}
static int ci_compare(const char *a,const char *b){while(*a&&*b){int ca=tolower((unsigned char)*a),cb=tolower((unsigned char)*b);if(ca!=cb)return ca-cb;a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
static int flag_get(save_editor_data_t*d,int id){return(d->event_flags[id>>3]>>(id&7))&1;}

static char amberscript_flag_names[PKS_EVENT_FLAGS_COUNT][96];
static int amberscript_flag_names_loaded;

static void readable_map_name(const char *src, char *dst, size_t size) {
    size_t n = 0;
    for (size_t i = 0; src[i] && n + 1 < size; i++) {
        unsigned char c = (unsigned char)src[i];
        unsigned char prev = i ? (unsigned char)src[i - 1] : 0;
        unsigned char next = (unsigned char)src[i + 1];
        int split_upper = i && isupper(c) &&
            (islower(prev) || (isupper(prev) && next && islower(next)));
        int split_digit = i && isdigit(c) && islower(prev);
        if ((split_upper || split_digit) && n + 2 < size) dst[n++] = ' ';
        dst[n++] = (char)c;
    }
    dst[n] = '\0';
}

static void load_amberscript_flag_names(void) {
    char data_dir[1024], packaged_path[1200], line[256];
    const char *paths[3];
    FILE *f = NULL;
    if (amberscript_flag_names_loaded) return;
    amberscript_flag_names_loaded = 1;
    paths[0] = "mod_runtime/pks_flag_registry.txt";
    paths[1] = "../mod_runtime/pks_flag_registry.txt";
    paths[2] = NULL;
    if (DataDir_Get(data_dir, sizeof data_dir) &&
        (size_t)snprintf(packaged_path, sizeof packaged_path,
                         "%smod_runtime/pks_flag_registry.txt",
                         data_dir) < sizeof packaged_path)
        paths[2] = packaged_path;
    for (int i = 0; i < 3 && !f; i++)
        if (paths[i]) f = fopen(paths[i], "rb");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char map[128], readable[160];
        int kind, declaration, id;
        const char *object;
        if (sscanf(line, "%127s %d %d %d", map, &kind, &declaration, &id) != 4)
            continue;
        if (id < PKS_EVENT_FLAGS_BASE || id >= NUM_EVENTS) continue;
        if (Map_RealIdForName(map) < 0) continue;
        if (kind == 0) object = "Trainer";
        else if (kind == 1) object = "Item Ball";
        else if (kind == 2) object = "Hidden Item";
        else if (kind == 3) object = "Hidden Coin";
        else object = "Map Event";
        readable_map_name(map, readable, sizeof readable);
        snprintf(amberscript_flag_names[id - PKS_EVENT_FLAGS_BASE],
                 sizeof amberscript_flag_names[0], "%s - %s %d",
                 readable, object, declaration + 1);
    }
    fclose(f);
}

static const char *flag_name_for_id(int id) {
    load_amberscript_flag_names();
    for (int i = 0; i < (int)NUM_EVENT_FLAG_IDS; i++)
        if ((int)kEventFlagIds[i].id == id) return kEventFlagIds[i].name;
    if (id >= PKS_EVENT_FLAGS_BASE && id < NUM_EVENTS &&
        amberscript_flag_names[id - PKS_EVENT_FLAGS_BASE][0])
        return amberscript_flag_names[id - PKS_EVENT_FLAGS_BASE];
    return NULL;
}

static void commit_u8(save_editor_t*e,void*ctx,void*ctx2,int aux,uint32_t value,const char*text){(void)ctx2;(void)aux;(void)text;uint8_t v=(uint8_t)value;memcpy(ctx,&v,1);e->dirty=1;}
static void commit_u16(save_editor_t*e,void*ctx,void*ctx2,int aux,uint32_t value,const char*text){(void)ctx2;(void)aux;(void)text;uint16_t v=(uint16_t)value;memcpy(ctx,&v,2);e->dirty=1;}
static void commit_u32(save_editor_t*e,void*ctx,void*ctx2,int aux,uint32_t value,const char*text){(void)ctx2;(void)aux;(void)text;memcpy(ctx,&value,4);e->dirty=1;}
static void commit_name(save_editor_t*e,void*ctx,void*ctx2,int aux,uint32_t value,const char*text){(void)ctx2;(void)aux;(void)value;SE_EncodeName(text,(uint8_t*)ctx);e->dirty=1;}
static void commit_search(save_editor_t*e,void*ctx,void*ctx2,int aux,uint32_t value,const char*text){(void)e;(void)ctx2;(void)aux;(void)value;snprintf((char*)ctx,64,"%s",text);}

static void events_tab(save_editor_t *e,unsigned in){
    static int scroll=0,sort_col=0,sort_desc=0;
    int ids[NUM_EVENTS],n=0;
    SDL_Rect search={108,112,300,28};
    const char *query=inline_owns_rect(search)?inline_edit.buffer:e->search;
    for(int id=0;id<NUM_EVENTS;id++){const char*name=flag_name_for_id(id);if(name&&ci_contains(name,query))ids[n++]=id;}
    int table_w=LDRAW_W-66,state_x=LDRAW_W-120;
    SDL_Rect columns[3]={{42,150,60,24},{102,150,state_x-102,24},{state_x,150,table_w-(state_x-42),24}};
    if(e->focus==1){if(in&LNAV_LEFT)sort_col=(sort_col+2)%3;if(in&LNAV_RIGHT)sort_col=(sort_col+1)%3;if(in&LNAV_ACCEPT)sort_desc=!sort_desc;}
    if(e->nav->ptr_pressed)for(int c=0;c<3;c++)if(hit(e,columns[c])){if(sort_col==c)sort_desc=!sort_desc;else{sort_col=c;sort_desc=0;}e->focus=1;}
    for(int i=1;i<n;i++){
        int value=ids[i],j=i-1;
        while(j>=0){
            int cmp=sort_col==0?ids[j]-value:sort_col==1?ci_compare(flag_name_for_id(ids[j]),flag_name_for_id(value)):flag_get(&e->data,ids[j])-flag_get(&e->data,value);
            if(!cmp)cmp=ids[j]-value;
            if(sort_desc)cmp=-cmp;
            if(cmp<=0)break;
            ids[j+1]=ids[j];j--;
        }
        ids[j+1]=value;
    }
    int maxfocus=n+1;if(in&LNAV_UP)e->focus--;if(in&LNAV_DOWN)e->focus++;if(e->focus<0)e->focus=0;if(e->focus>maxfocus)e->focus=maxfocus;
    char shown_search[72];Uint8 search_c=e->focus==0?255:0;snprintf(shown_search,sizeof shown_search,"%s%s",query,inline_owns_rect(search)?"_":"");LauncherDraw_TextBold(e->r,42,LDRAW_TEXT_Y(112,28,1),1,LCOL_TEXT,"SEARCH:");LauncherDraw_Bevel(e->r,search,0);if(e->focus==0){SDL_Rect f={search.x+3,search.y+3,search.w-6,search.h-6};LauncherDraw_FocusBar(e->r,f);}LauncherDraw_TextClippedBold(e->r,search.x+7,LDRAW_TEXT_Y(search.y,search.h,1),1,search_c,search_c,search_c,shown_search,search.w-14);
    if((e->nav->ptr_pressed&&hit(e,search))||(e->focus==0&&(in&LNAV_ACCEPT))){e->focus=0;begin_inline_ascii(search,e->search,commit_search);scroll=0;}
    int rows=(LDRAW_H-216)/24;if(rows<4)rows=4;if(e->focus>=2){int s=e->focus-2;if(s<scroll)scroll=s;if(s>=scroll+rows)scroll=s-rows+1;}if(scroll<0)scroll=0;if(scroll>n-rows)scroll=n>rows?n-rows:0;
    SDL_Rect head={42,150,table_w,24};LauncherDraw_Bevel(e->r,head,0);if(e->focus==1)LauncherDraw_FocusBar(e->r,columns[sort_col]);
    char id_head[20],event_head[20],state_head[20];snprintf(id_head,sizeof id_head,"ID%s",sort_col==0?(sort_desc?" DESC":" ASC"):"");snprintf(event_head,sizeof event_head,"EVENT%s",sort_col==1?(sort_desc?" DESC":" ASC"):"");snprintf(state_head,sizeof state_head,"STATE%s",sort_col==2?(sort_desc?" DESC":" ASC"):"");Uint8 hc=e->focus==1?255:0;LauncherDraw_TextBold(e->r,50,158,1,sort_col==0&&e->focus==1?hc:0,sort_col==0&&e->focus==1?hc:0,sort_col==0&&e->focus==1?hc:0,id_head);LauncherDraw_TextBold(e->r,110,158,1,sort_col==1&&e->focus==1?hc:0,sort_col==1&&e->focus==1?hc:0,sort_col==1&&e->focus==1?hc:0,event_head);LauncherDraw_TextBold(e->r,state_x,158,1,sort_col==2&&e->focus==1?hc:0,sort_col==2&&e->focus==1?hc:0,sort_col==2&&e->focus==1?hc:0,state_head);
    for(int row=0;row<rows&&scroll+row<n;row++){int id=ids[scroll+row],cid=scroll+row+2;const char*name=flag_name_for_id(id);if(!name)continue;SDL_Rect rr={42,174+row*24,table_w,24};int foc=e->focus==cid;if(control(e,in,cid,rr)){e->data.event_flags[id>>3]^=(uint8_t)(1u<<(id&7));e->dirty=1;}if(foc)LauncherDraw_FocusBar(e->r,rr);Uint8 c=foc?255:0;char num[16];snprintf(num,sizeof(num),"%04d",id);LauncherDraw_TextBold(e->r,50,LDRAW_TEXT_Y(rr.y,rr.h,1),1,c,c,c,num);LauncherDraw_TextClippedBold(e->r,110,LDRAW_TEXT_Y(rr.y,rr.h,1),1,c,c,c,name,state_x-122);LauncherDraw_TextBold(e->r,state_x,LDRAW_TEXT_Y(rr.y,rr.h,1),1,c,c,c,flag_get(&e->data,id)?"SET":"CLEAR");}
}

static const char*badge_names[8]={"BOULDER","CASCADE","THUNDER","RAINBOW","SOUL","MARSH","VOLCANO","EARTH"};
static void player_tab(save_editor_t*e,unsigned in){
    if(in&LNAV_UP)e->focus--;
    if(in&LNAV_DOWN)e->focus++;
    if(e->focus<0)e->focus=0;
    if(e->focus>14)e->focus=14;
    char v[64];SDL_Rect r;
    SE_DecodeName(e->data.player_name,v,sizeof(v));field(e,116,"PLAYER NAME:",v,e->focus==0,0);r=(SDL_Rect){210,116,300,24};if(control(e,in,0,r))begin_inline_name(e,r,"PLAYER NAME",e->data.player_name,commit_name);
    SE_DecodeName(e->data.rival_name,v,sizeof(v));field(e,148,"RIVAL NAME:",v,e->focus==1,0);r=(SDL_Rect){210,148,300,24};if(control(e,in,1,r))begin_inline_name(e,r,"RIVAL NAME",e->data.rival_name,commit_name);
    snprintf(v,sizeof(v),"%u",e->data.player_id);field(e,180,"TRAINER ID:",v,e->focus==2,0);r=(SDL_Rect){210,180,300,24};if(control(e,in,2,r))begin_inline_number(e,r,"TRAINER ID",e->data.player_id,0,65535,commit_u16,&e->data.player_id,NULL,0);
    snprintf(v,sizeof(v),"%u",e->data.money);field(e,212,"MONEY:",v,e->focus==3,0);r=(SDL_Rect){210,212,300,24};if(control(e,in,3,r))begin_inline_number(e,r,"MONEY",e->data.money,0,999999,commit_u32,&e->data.money,NULL,0);
    {
        const char *name=SE_LocationCurrentName(&e->data);
        int count=SE_LocationCount();
        int current=SE_LocationFind(name);
        if(name)snprintf(v,sizeof(v),"%s",name);
        else if(e->data.cur_map>=248)snprintf(v,sizeof(v),"UNBOUND VMAP SLOT %u",e->data.cur_map);
        else snprintf(v,sizeof(v),"LEGACY MAP %u",e->data.cur_map);
        field(e,244,"CURRENT LOCATION:",v,e->focus==4,count>0);
        r=(SDL_Rect){210,244,300,24};
        if(count>0&&control(e,in,4,r))open_workspace_dropdown(e,WDD_LOCATION,0,4,r,count,current,SE_LocationLabel,NULL,NULL);
    }
    {
        uint32_t max_x=255,max_y=255;
        SE_LocationCurrentBounds(&e->data,&max_x,&max_y);
        snprintf(v,sizeof(v),"%u",e->data.x_coord);field(e,276,"X COORDINATE:",v,e->focus==5,0);r=(SDL_Rect){210,276,300,24};if(control(e,in,5,r))begin_inline_number(e,r,"X COORDINATE",e->data.x_coord,0,max_x,commit_u8,&e->data.x_coord,NULL,0);
        snprintf(v,sizeof(v),"%u",e->data.y_coord);field(e,308,"Y COORDINATE:",v,e->focus==6,0);r=(SDL_Rect){210,308,300,24};if(control(e,in,6,r))begin_inline_number(e,r,"Y COORDINATE",e->data.y_coord,0,max_y,commit_u8,&e->data.y_coord,NULL,0);
    }
    LauncherDraw_TextBold(e->r,42,360,1,LCOL_TEXT,"BADGES:");for(int i=0;i<8;i++){r=(SDL_Rect){130+(i%4)*155,350+(i/4)*34,145,26};checkbox(e,r,badge_names[i],(e->data.badges>>i)&1,e->focus==7+i);if(control(e,in,7+i,r)){e->data.badges^=(uint8_t)(1u<<i);e->dirty=1;}}
    LauncherDraw_TextBold(e->r,250,440,1,LCOL_TEXT_DIM,
                          "CHANGES ARE STAGED UNTIL YOU CLICK SAVE");
}

static void dex_tab(save_editor_t*e,unsigned in){
    static int scroll=0,col=0,sort_col=0,sort_desc=0;
    static char search_text[64];
    int dexes[151],count=0;
    SDL_Rect all={42,112,150,28},clear={200,112,110,28};
    SDL_Rect search={408,112,LDRAW_W-450,28};
    if(search.w<180)search.w=180;
    const char *query=inline_owns_rect(search)?inline_edit.buffer:search_text;
    for(int dex=1;dex<=151;dex++)if(ci_contains(kPokemonNames[dex],query))dexes[count++]=dex;
    int table_w=LDRAW_W-66,seen_x=LDRAW_W-100,owned_x=seen_x-100;
    SDL_Rect columns[4]={{42,150,63,24},{105,150,owned_x-105,24},{owned_x-8,150,100,24},{seen_x-8,150,table_w-(seen_x-50),24}};
    if(e->focus==3){if(in&LNAV_LEFT)sort_col=(sort_col+3)%4;if(in&LNAV_RIGHT)sort_col=(sort_col+1)%4;if(in&LNAV_ACCEPT)sort_desc=!sort_desc;}
    else if(e->focus>=4){if(in&LNAV_LEFT)col=0;if(in&LNAV_RIGHT)col=1;}
    if(e->nav->ptr_pressed)for(int c=0;c<4;c++)if(hit(e,columns[c])){if(sort_col==c)sort_desc=!sort_desc;else{sort_col=c;sort_desc=0;}e->focus=3;}
    for(int i=1;i<count;i++){
        int value=dexes[i],j=i-1;
        while(j>=0){
            int a=dexes[j],cmp;
            if(sort_col==0)cmp=a-value;
            else if(sort_col==1)cmp=ci_compare(kPokemonNames[a],kPokemonNames[value]);
            else if(sort_col==2)cmp=((e->data.pokedex_owned[(a-1)>>3]>>((a-1)&7))&1)-((e->data.pokedex_owned[(value-1)>>3]>>((value-1)&7))&1);
            else cmp=((e->data.pokedex_seen[(a-1)>>3]>>((a-1)&7))&1)-((e->data.pokedex_seen[(value-1)>>3]>>((value-1)&7))&1);
            if(!cmp)cmp=a-value;
            if(sort_desc)cmp=-cmp;
            if(cmp<=0)break;
            dexes[j+1]=dexes[j];j--;
        }
        dexes[j+1]=value;
    }
    if(in&LNAV_UP)e->focus--;
    if(in&LNAV_DOWN)e->focus++;
    if(e->focus<0)e->focus=0;
    if(e->focus>count+3)e->focus=count+3;
    button(e,all,"MARK ALL",e->focus==0);button(e,clear,"CLEAR ALL",e->focus==1);
    if(control(e,in,0,all)){memset(e->data.pokedex_owned,0xFF,19);memset(e->data.pokedex_seen,0xFF,19);e->dirty=1;}
    if(control(e,in,1,clear)){memset(e->data.pokedex_owned,0,19);memset(e->data.pokedex_seen,0,19);e->dirty=1;}
    char shown_search[72];Uint8 search_c=e->focus==2?255:0;snprintf(shown_search,sizeof shown_search,"%s%s",query,inline_owns_rect(search)?"_":"");LauncherDraw_TextBold(e->r,326,LDRAW_TEXT_Y(112,28,1),1,LCOL_TEXT,"SEARCH:");LauncherDraw_Bevel(e->r,search,0);if(e->focus==2){SDL_Rect f={search.x+3,search.y+3,search.w-6,search.h-6};LauncherDraw_FocusBar(e->r,f);}LauncherDraw_TextClippedBold(e->r,search.x+7,LDRAW_TEXT_Y(search.y,search.h,1),1,search_c,search_c,search_c,shown_search,search.w-14);
    if((e->nav->ptr_pressed&&hit(e,search))||(e->focus==2&&(in&LNAV_ACCEPT))){e->focus=2;begin_inline_ascii(search,search_text,commit_search);scroll=0;}
    int rows=(LDRAW_H-216)/24;if(rows<4)rows=4;if(rows>151)rows=151;
    if(e->focus>=4){int s=e->focus-4;if(s<scroll)scroll=s;if(s>=scroll+rows)scroll=s-rows+1;}if(scroll<0)scroll=0;if(scroll>count-rows)scroll=count>rows?count-rows:0;
    SDL_Rect h={42,150,table_w,24};LauncherDraw_Bevel(e->r,h,0);if(e->focus==3)LauncherDraw_FocusBar(e->r,columns[sort_col]);
    const char *base_headers[4]={"DEX","SPECIES","CAUGHT","SEEN"};char headers[4][24];for(int c=0;c<4;c++)snprintf(headers[c],sizeof headers[c],"%s%s",base_headers[c],sort_col==c?(sort_desc?" DESC":" ASC"):"");int hx[4]={50,105,owned_x,seen_x};for(int c=0;c<4;c++){Uint8 tc=e->focus==3&&sort_col==c?255:0;LauncherDraw_TextBold(e->r,hx[c],158,1,tc,tc,tc,headers[c]);}
    for(int i=0;i<rows&&scroll+i<151;i++){
        if(scroll+i>=count)break;
        int dex=dexes[scroll+i],id=scroll+i+4;
        SDL_Rect rr={42,174+i*24,table_w,24};
        if(e->nav->ptr_pressed&&hit(e,rr)&&e->nav->ptr_x>=owned_x-8)col=e->nav->ptr_x>(owned_x+seen_x)/2?1:0;
        int act=control(e,in,id,rr);
        int bit=dex-1;
        int owned=(e->data.pokedex_owned[bit>>3]>>(bit&7))&1;
        int seen=(e->data.pokedex_seen[bit>>3]>>(bit&7))&1;
        if(act){
            uint8_t m=(uint8_t)(1u<<(bit&7));
            if(col==0){
                e->data.pokedex_owned[bit>>3]^=m;
                if(e->data.pokedex_owned[bit>>3]&m)e->data.pokedex_seen[bit>>3]|=m;
            }else{
                e->data.pokedex_seen[bit>>3]^=m;
                if(!(e->data.pokedex_seen[bit>>3]&m))e->data.pokedex_owned[bit>>3]&=(uint8_t)~m;
            }
            e->dirty=1;
        }
        int foc=e->focus==id;
        SDL_Rect active={col==0?owned_x-8:seen_x-8,rr.y,88,rr.h};
        if(foc)LauncherDraw_FocusBar(e->r,active);
        char num[8];
        snprintf(num,sizeof(num),"%03d",dex);
        LauncherDraw_TextBold(e->r,50,LDRAW_TEXT_Y(rr.y,rr.h,1),1,LCOL_TEXT,num);
        LauncherDraw_TextClippedBold(e->r,105,LDRAW_TEXT_Y(rr.y,rr.h,1),1,
                                     LCOL_TEXT,kPokemonNames[dex],owned_x-120);
        SDL_Rect owned_box={owned_x,rr.y+4,16,16};
        SDL_Rect seen_box={seen_x,rr.y+4,16,16};
        LauncherDraw_Bevel(e->r,owned_box,0);
        LauncherDraw_Bevel(e->r,seen_box,0);
        if(owned)LauncherDraw_TextBold(e->r,owned_box.x+4,owned_box.y+3,1,LCOL_TEXT,"X");
        if(seen)LauncherDraw_TextBold(e->r,seen_box.x+4,seen_box.y+3,1,LCOL_TEXT,"X");
    }
}

static const char*species_label(void*ctx,int i){(void)ctx;return kPokemonNames[i+1];}
static const char*move_label(void*ctx,int i){(void)ctx;return i&&gMoveNames[i]?gMoveNames[i]:"NONE";}
static const uint8_t status_values[] = { 0, 1, 8, 16, 32, 64 };
static const char *status_name(uint8_t status) {
    if (status == 0) return "HEALTHY";
    if (status & 7) return "ASLEEP";
    if (status & 8) return "POISONED";
    if (status & 16) return "BURNED";
    if (status & 32) return "FROZEN";
    if (status & 64) return "PARALYZED";
    return "OTHER";
}
static const char *status_label(void *ctx, int i) { (void)ctx; return status_name(status_values[i]); }
static void commit_level(save_editor_t*e,void*ctx,void*ctx2,int party,uint32_t value,const char*text){(void)text;box_mon_t*m=ctx;m->box_level=(uint8_t)value;if(party)((party_mon_t*)ctx2)->level=(uint8_t)value;e->dirty=1;}
static void commit_sleep(save_editor_t*e,void*ctx,void*ctx2,int aux,uint32_t value,const char*text){(void)ctx2;(void)aux;(void)text;box_mon_t*m=ctx;if(m->status&7)m->status=(uint8_t)value;e->dirty=1;}
static void commit_exp(save_editor_t*e,void*ctx,void*ctx2,int aux,uint32_t value,const char*text){(void)ctx2;(void)aux;(void)text;box_mon_t*m=ctx;m->exp[0]=(uint8_t)(value>>16);m->exp[1]=(uint8_t)(value>>8);m->exp[2]=(uint8_t)value;e->dirty=1;}
static void commit_dv(save_editor_t*e,void*ctx,void*ctx2,int shift,uint32_t value,const char*text){(void)ctx2;(void)text;box_mon_t*m=ctx;m->dvs=(uint16_t)((m->dvs&~(15u<<shift))|(value<<shift));e->dirty=1;}
static void commit_stat_exp(save_editor_t*e,void*ctx,void*ctx2,int stat,uint32_t value,const char*text){(void)ctx2;(void)text;box_mon_t*m=ctx;if(stat==0)m->stat_exp_hp=(uint16_t)value;else if(stat==1)m->stat_exp_atk=(uint16_t)value;else if(stat==2)m->stat_exp_def=(uint16_t)value;else if(stat==3)m->stat_exp_spd=(uint16_t)value;else m->stat_exp_spc=(uint16_t)value;e->dirty=1;}
static void commit_pp(save_editor_t*e,void*ctx,void*ctx2,int move,uint32_t value,const char*text){(void)ctx2;(void)text;box_mon_t*m=ctx;m->pp[move]=(uint8_t)((m->pp[move]&0xC0)|value);e->dirty=1;}
static void commit_ppups(save_editor_t*e,void*ctx,void*ctx2,int move,uint32_t value,const char*text){(void)ctx2;(void)text;box_mon_t*m=ctx;m->pp[move]=(uint8_t)((m->pp[move]&0x3F)|(value<<6));e->dirty=1;}

static void mon_form(save_editor_t *e, unsigned in, box_mon_t *m,
                     uint8_t *nick, uint8_t *ot, int party,
                     party_mon_t *pm, int base_id) {
    enum { MON_FIELDS = 35 };
    char value[80];
    SDL_Rect r;
    int id;
    int step = LDRAW_H >= 580 ? 22 : 18;
    int left_y = 156;
    int right_x = LDRAW_W / 2 + 5;
    int right_field_w = LDRAW_W - right_x - 140;
    if (right_field_w < 255) right_field_w = 255;
    int moves_title_y = left_y + 5 * step + 8;
    int moves_y = moves_title_y + 14;
    if (in & LNAV_UP) e->focus--;
    if (in & LNAV_DOWN) e->focus++;
    if (e->focus < 0) e->focus = 0;
    if (e->focus >= base_id + MON_FIELDS) e->focus = base_id + MON_FIELDS - 1;
    if (!party && e->focus >= base_id + 6 && e->focus <= base_id + 10)
        e->focus = (in & LNAV_UP) ? base_id + 5 : base_id + 11;

    int dex = gSpeciesToDex[m->species];
    snprintf(value, sizeof(value), "%03d  %s", dex, dex ? kPokemonNames[dex] : "UNKNOWN");
    id = base_id;
    r = compact_field(e,42,left_y,96,225,"SPECIES",value,e->focus==id,1);
    if (control(e, in, id, r))
        open_workspace_dropdown(e,WDD_SPECIES,0,id,r,151,dex-1,species_label,NULL,m);

    SE_DecodeName(nick, value, sizeof(value));
    id=base_id+1;r=compact_field(e,42,left_y+step,96,225,"NICKNAME",value,e->focus==id,0);
    if (control(e, in, id, r)) begin_inline_name(e,r,"NICKNAME",nick,commit_name);
    SE_DecodeName(ot, value, sizeof(value));
    id=base_id+2;r=compact_field(e,42,left_y+2*step,96,225,"OT NAME",value,e->focus==id,0);
    if (control(e, in, id, r)) begin_inline_name(e,r,"OT NAME",ot,commit_name);

#define NUMBER_FIELD_AT(INDEX, X, Y, LABEL_W, FIELD_W, LABEL, CURRENT, MINIMUM, MAXIMUM, COMMIT, CTX, CTX2, AUX) \
    do { \
        snprintf(value, sizeof(value), "%u", (unsigned)(CURRENT)); \
        id = base_id + (INDEX); \
        r = compact_field(e, (X), (Y), (LABEL_W), (FIELD_W), (LABEL), value, e->focus == id, 0); \
        if (control(e, in, id, r)) begin_inline_number(e,r,(LABEL),(CURRENT),(MINIMUM),(MAXIMUM),(COMMIT),(CTX),(CTX2),(AUX)); \
    } while (0)

    NUMBER_FIELD_AT(3,42,left_y+3*step,96,225,"OT ID",m->ot_id,0,65535,commit_u16,&m->ot_id,NULL,0);
    uint8_t level = party ? pm->level : m->box_level;
    NUMBER_FIELD_AT(4,42,left_y+4*step,96,225,"LEVEL",level,1,100,commit_level,m,pm,party);
    NUMBER_FIELD_AT(5,42,left_y+5*step,96,225,"CURRENT HP",m->hp,0,65535,commit_u16,&m->hp,NULL,0);

    if (party) {
        NUMBER_FIELD_AT(6,42,left_y+6*step,96,225,"MAX HP",pm->max_hp,0,65535,commit_u16,&pm->max_hp,NULL,0);
        NUMBER_FIELD_AT(7,42,left_y+7*step,96,225,"ATTACK",pm->atk,0,65535,commit_u16,&pm->atk,NULL,0);
        NUMBER_FIELD_AT(8,42,left_y+8*step,96,225,"DEFENSE",pm->def,0,65535,commit_u16,&pm->def,NULL,0);
        NUMBER_FIELD_AT(9,42,left_y+9*step,96,225,"SPEED",pm->spd,0,65535,commit_u16,&pm->spd,NULL,0);
        NUMBER_FIELD_AT(10,42,left_y+10*step,96,225,"SPECIAL",pm->spc,0,65535,commit_u16,&pm->spc,NULL,0);
    } else {
        compact_field(e,42,left_y+6*step,96,225,"MAX HP","PARTY ONLY",0,0);
        compact_field(e,42,left_y+7*step,96,225,"ATTACK","PARTY ONLY",0,0);
        compact_field(e,42,left_y+8*step,96,225,"DEFENSE","PARTY ONLY",0,0);
        compact_field(e,42,left_y+9*step,96,225,"SPEED","PARTY ONLY",0,0);
        compact_field(e,42,left_y+10*step,96,225,"SPECIAL","PARTY ONLY",0,0);
    }

    id = base_id + 11;
    r=compact_field(e,42,left_y+11*step,96,225,"STATUS",status_name(m->status),e->focus==id,1);
    if (control(e, in, id, r)) {
        int selected = 0;
        for (int i = 0; i < 6; i++) if (status_values[i] == m->status) selected = i;
        open_workspace_dropdown(e,WDD_STATUS,0,id,r,6,selected,status_label,NULL,m);
    }

    NUMBER_FIELD_AT(12,42,left_y+12*step,96,225,"SLEEP TURNS",m->status&7,1,7,commit_sleep,m,NULL,0);

    uint32_t exp = ((uint32_t)m->exp[0] << 16) | ((uint32_t)m->exp[1] << 8) | m->exp[2];
    NUMBER_FIELD_AT(13,42,left_y+13*step,96,225,"EXPERIENCE",exp,0,0xFFFFFF,commit_exp,m,NULL,0);

    static const char *dv_names[4] = { "ATTACK DV", "DEFENSE DV", "SPEED DV", "SPECIAL DV" };
    for (int i = 0; i < 4; i++) {
        int shift = (3 - i) * 4;
        uint32_t current = (m->dvs >> shift) & 15;
        snprintf(value, sizeof(value), "%u", (unsigned)current);
        id = base_id + 14 + i;
        r=compact_field(e,42,left_y+(14+i)*step,96,225,dv_names[i],value,e->focus==id,0);
        if(control(e,in,id,r))begin_inline_number(e,r,dv_names[i],current,0,15,commit_dv,m,NULL,shift);
    }

    static const char *stat_names[5] = { "HP STAT EXP", "ATK STAT EXP", "DEF STAT EXP", "SPD STAT EXP", "SPC STAT EXP" };
    for (int i = 0; i < 5; i++) {
        uint16_t current = i == 0 ? m->stat_exp_hp : i == 1 ? m->stat_exp_atk :
                           i == 2 ? m->stat_exp_def : i == 3 ? m->stat_exp_spd : m->stat_exp_spc;
        snprintf(value, sizeof(value), "%u", (unsigned)current);
        id = base_id + 18 + i;
        r=compact_field(e,right_x,left_y+i*step,102,right_field_w,stat_names[i],value,e->focus==id,0);
        if(control(e,in,id,r))begin_inline_number(e,r,stat_names[i],current,0,65535,commit_stat_exp,m,NULL,i);
    }

    LauncherDraw_TextBold(e->r,right_x,moves_title_y,1,LCOL_TEXT,"MOVES");
    for (int i = 0; i < 4; i++) {
        int move = m->moves[i];
        snprintf(value, sizeof(value), "%03d  %s", move,
                 move && gMoveNames[move] ? gMoveNames[move] : "NONE");
        id = base_id + 23 + i * 3;
        char label[16]; snprintf(label, sizeof(label), "MOVE %d", i + 1);
        r=compact_field(e,right_x,moves_y+i*step*3,102,right_field_w,label,value,e->focus==id,1);
        if (control(e, in, id, r))
            open_workspace_dropdown(e,WDD_MOVE,i,id,r,NUM_MOVE_DEFS,move,move_label,NULL,m);
    }

    for (int i = 0; i < 4; i++) {
        char label[16]; snprintf(label, sizeof(label), "PP %d", i + 1);
        snprintf(value, sizeof(value), "%u", (unsigned)(m->pp[i] & 0x3F));
        id = base_id + 24 + i * 3;
        r=compact_field(e,right_x,moves_y+step+i*step*3,102,80,label,value,e->focus==id,0);
        if(control(e,in,id,r))begin_inline_number(e,r,label,m->pp[i]&0x3F,0,63,commit_pp,m,NULL,i);
    }
    for (int i = 0; i < 4; i++) {
        char label[16]; snprintf(label, sizeof(label), "PP-UPS %d", i + 1);
        snprintf(value, sizeof(value), "%u", (unsigned)(m->pp[i] >> 6));
        id = base_id + 25 + i * 3;
        r=compact_field(e,right_x+189,moves_y+step+i*step*3,84,84,label,value,e->focus==id,0);
        if(control(e,in,id,r))begin_inline_number(e,r,label,m->pp[i]>>6,0,3,commit_ppups,m,NULL,i);
    }
#undef NUMBER_FIELD_AT
}

static const char*slot_label(void*ctx,int i){(void)ctx;static char b[BOX_CAPACITY][16];snprintf(b[i],16,"SLOT %d",i+1);return b[i];}
static const char*box_label(void*ctx,int i){(void)ctx;static char b[12][16];snprintf(b[i%12],16,"BOX %d",i+1);return b[i%12];}

static void init_box_mon(save_editor_t *e, int box, int slot) {
    const int dex = 1;
    const int level = 5;
    const base_stats_t *base = &gBaseStats[dex];
    box_mon_t *mon = &e->data.box_mons[box][slot];
    memset(mon, 0, sizeof(*mon));
    mon->species = gDexToSpecies[dex];
    mon->box_level = level;
    mon->type1 = base->type1;
    mon->type2 = base->type2;
    mon->catch_rate = base->catch_rate;
    mon->ot_id = e->data.player_id;
    for (int i = 0; i < 4; i++) {
        mon->moves[i] = base->start_moves[i];
        mon->pp[i] = base->start_moves[i] ? gMoves[base->start_moves[i]].pp : 0;
    }
    uint32_t exp = CalcExpForLevel(base->growth_rate, level);
    mon->exp[0] = (uint8_t)(exp >> 16);
    mon->exp[1] = (uint8_t)(exp >> 8);
    mon->exp[2] = (uint8_t)exp;
    SE_EncodeName(kPokemonNames[dex], e->data.box_nicks[box][slot]);
    memcpy(e->data.box_ot[box][slot], e->data.player_name, NAME_LENGTH);
}

static void delete_box_mon(save_editor_t *e, int box, int slot) {
    int count=e->data.box_count[box];
    if(slot<0||slot>=count)return;
    for(int i=slot;i+1<count;i++){
        e->data.box_mons[box][i]=e->data.box_mons[box][i+1];
        memcpy(e->data.box_nicks[box][i],e->data.box_nicks[box][i+1],NAME_LENGTH);
        memcpy(e->data.box_ot[box][i],e->data.box_ot[box][i+1],NAME_LENGTH);
    }
    memset(&e->data.box_mons[box][count-1],0,sizeof(box_mon_t));
    memset(e->data.box_nicks[box][count-1],0x50,NAME_LENGTH);
    memset(e->data.box_ot[box][count-1],0x50,NAME_LENGTH);
    e->data.box_count[box]=(uint8_t)(count-1);
    if(e->box_slot>=e->data.box_count[box]&&e->box_slot>0)e->box_slot--;
    e->dirty=1;
}

static void box_list_tab(save_editor_t *e, unsigned in) {
    static int sort_col=0,sort_desc=0;
    static char search_text[64];
    char label[64];
    int count = e->data.box_count[e->box_num];
    int slots[BOX_CAPACITY],shown_count=0;
    SDL_Rect search={108,150,300,28};
    const char *query=inline_owns_rect(search)?inline_edit.buffer:search_text;
    for(int slot=0;slot<count;slot++){
        char nick[32];int dex=gSpeciesToDex[e->data.box_mons[e->box_num][slot].species];
        SE_DecodeName(e->data.box_nicks[e->box_num][slot],nick,sizeof nick);
        if(ci_contains(nick,query)||ci_contains(dex?kPokemonNames[dex]:"EMPTY",query))slots[shown_count++]=slot;
    }
    int table_w=LDRAW_W-66,level_x=LDRAW_W-120;
    SDL_Rect columns[4]={{42,188,60,24},{102,188,228,24},{330,188,level_x-330,24},{level_x,188,table_w-(level_x-42),24}};
    if(e->focus==4){if(in&LNAV_LEFT)sort_col=(sort_col+3)%4;if(in&LNAV_RIGHT)sort_col=(sort_col+1)%4;if(in&LNAV_ACCEPT)sort_desc=!sort_desc;}
    if(e->nav->ptr_pressed)for(int c=0;c<4;c++)if(hit(e,columns[c])){if(sort_col==c)sort_desc=!sort_desc;else{sort_col=c;sort_desc=0;}e->focus=4;}
    for(int i=1;i<shown_count;i++){
        int value=slots[i],j=i-1;
        while(j>=0){
            int a=slots[j],cmp=0;
            if(sort_col==0)cmp=a-value;
            else if(sort_col==1){char an[32],bn[32];SE_DecodeName(e->data.box_nicks[e->box_num][a],an,sizeof an);SE_DecodeName(e->data.box_nicks[e->box_num][value],bn,sizeof bn);cmp=ci_compare(an,bn);}
            else if(sort_col==2){int ad=gSpeciesToDex[e->data.box_mons[e->box_num][a].species],bd=gSpeciesToDex[e->data.box_mons[e->box_num][value].species];cmp=ci_compare(ad?kPokemonNames[ad]:"EMPTY",bd?kPokemonNames[bd]:"EMPTY");}
            else cmp=(int)e->data.box_mons[e->box_num][a].box_level-(int)e->data.box_mons[e->box_num][value].box_level;
            if(!cmp)cmp=a-value;
            if(sort_desc)cmp=-cmp;
            if(cmp<=0)break;
            slots[j+1]=slots[j];j--;
        }
        slots[j+1]=value;
    }
    int max_focus = shown_count + 4;
    if (in & LNAV_UP) e->focus--;
    if (in & LNAV_DOWN) e->focus++;
    if (e->focus < 0) e->focus = 0;
    if (e->focus > max_focus) e->focus = max_focus;

    SDL_Rect box = {42,112,150,30};
    snprintf(label,sizeof(label),"BOX: %d OF %d",e->box_num+1,NUM_BOXES);
    LauncherDropdown_DrawField(e->r,box,label,e->focus==0,dropdown_owns_rect(box));
    if(control(e,in,0,box))
        open_workspace_dropdown(e,WDD_BOX,0,0,box,NUM_BOXES,e->box_num,box_label,NULL,NULL);

    SDL_Rect add = {210,112,150,30};
    button(e,add,count<BOX_CAPACITY?"ADD POKEMON":"BOX FULL",e->focus==1);
    if(control(e,in,1,add)&&count<BOX_CAPACITY){
        init_box_mon(e,e->box_num,count);
        e->data.box_count[e->box_num]++;
        e->box_slot=count;
        e->box_detail=1;
        e->focus=0;
        e->dirty=1;
        return;
    }
    SDL_Rect del={370,112,180,30};
    button(e,del,count?"DELETE SELECTED":"NOTHING TO DELETE",e->focus==2);
    if(control(e,in,2,del)&&count){delete_box_mon(e,e->box_num,e->box_slot);e->focus=0;e->scroll=0;return;}
    snprintf(label,sizeof(label),"%d / %d STORED",count,BOX_CAPACITY);
    LauncherDraw_TextBold(e->r,570,LDRAW_TEXT_Y(112,30,1),1,LCOL_TEXT_DIM,label);

    char shown_search[72];Uint8 search_c=e->focus==3?255:0;snprintf(shown_search,sizeof shown_search,"%s%s",query,inline_owns_rect(search)?"_":"");LauncherDraw_TextBold(e->r,42,LDRAW_TEXT_Y(150,28,1),1,LCOL_TEXT,"SEARCH:");LauncherDraw_Bevel(e->r,search,0);if(e->focus==3){SDL_Rect f={search.x+3,search.y+3,search.w-6,search.h-6};LauncherDraw_FocusBar(e->r,f);}LauncherDraw_TextClippedBold(e->r,search.x+7,LDRAW_TEXT_Y(search.y,search.h,1),1,search_c,search_c,search_c,shown_search,search.w-14);
    if((e->nav->ptr_pressed&&hit(e,search))||(e->focus==3&&(in&LNAV_ACCEPT))){e->focus=3;begin_inline_ascii(search,search_text,commit_search);e->scroll=0;}
    SDL_Rect head={42,188,table_w,24};LauncherDraw_Bevel(e->r,head,0);if(e->focus==4)LauncherDraw_FocusBar(e->r,columns[sort_col]);
    const char *base_headers[4]={"SLOT","NICKNAME","SPECIES","LEVEL"};char headers[4][24];for(int c=0;c<4;c++)snprintf(headers[c],sizeof headers[c],"%s%s",base_headers[c],sort_col==c?(sort_desc?" DESC":" ASC"):"");int hx[4]={50,110,330,level_x};for(int c=0;c<4;c++){Uint8 tc=e->focus==4&&sort_col==c?255:0;LauncherDraw_TextBold(e->r,hx[c],196,1,tc,tc,tc,headers[c]);}
    int rows=(LDRAW_H-254)/24;if(rows<4)rows=4;
    if(e->focus>=5){int selected=e->focus-5;e->box_slot=slots[selected];if(selected<e->scroll)e->scroll=selected;if(selected>=e->scroll+rows)e->scroll=selected-rows+1;}
    if(e->scroll<0)e->scroll=0;
    if(e->scroll>shown_count-rows)e->scroll=shown_count>rows?shown_count-rows:0;
    for(int row=0;row<rows&&e->scroll+row<shown_count;row++){
        int display=e->scroll+row,slot=slots[display],id=display+5,dex=gSpeciesToDex[e->data.box_mons[e->box_num][slot].species];
        SDL_Rect rr={42,212+row*24,table_w,24};
        if(control(e,in,id,rr)){e->box_slot=slot;e->box_detail=1;e->focus=0;return;}
        int focused=e->focus==id;if(focused)LauncherDraw_FocusBar(e->r,rr);Uint8 c=focused?255:0;
        char number[16],nick[32],level[16];snprintf(number,sizeof(number),"%02d",slot+1);SE_DecodeName(e->data.box_nicks[e->box_num][slot],nick,sizeof(nick));snprintf(level,sizeof(level),"%u",e->data.box_mons[e->box_num][slot].box_level);
        LauncherDraw_TextBold(e->r,50,LDRAW_TEXT_Y(rr.y,rr.h,1),1,c,c,c,number);
        LauncherDraw_TextClippedBold(e->r,110,LDRAW_TEXT_Y(rr.y,rr.h,1),1,c,c,c,nick,200);
        LauncherDraw_TextClippedBold(e->r,330,LDRAW_TEXT_Y(rr.y,rr.h,1),1,c,c,c,dex?kPokemonNames[dex]:"EMPTY",level_x-342);
        LauncherDraw_TextBold(e->r,level_x,LDRAW_TEXT_Y(rr.y,rr.h,1),1,c,c,c,level);
    }
    if(!count)LauncherDraw_TextBold(e->r,270,320,2,LCOL_TEXT_DIM,"THIS BOX IS EMPTY");
    else if(!shown_count)LauncherDraw_TextBold(e->r,270,320,2,LCOL_TEXT_DIM,"NO MATCHES");
}

static void party_tab(save_editor_t*e,unsigned in){
    char label[48];SDL_Rect size={42,112,150,30},slot={220,112,150,30};
    snprintf(label,sizeof(label),"PARTY SIZE: %u",e->data.party_count);button(e,size,label,e->focus==0);
    snprintf(label,sizeof(label),"SLOT: %d OF %d",e->party_slot+1,PARTY_LENGTH);
    LauncherDropdown_DrawField(e->r,slot,label,e->focus==1,dropdown_owns_rect(slot));
    if(control(e,in,0,size))begin_inline_number(e,size,"PARTY SIZE",e->data.party_count,0,6,commit_u8,&e->data.party_count,NULL,0);
    if(control(e,in,1,slot))open_workspace_dropdown(e,WDD_PARTY_SLOT,0,1,slot,PARTY_LENGTH,e->party_slot,slot_label,NULL,NULL);
    if(e->party_slot>=PARTY_LENGTH)e->party_slot=0;
    mon_form(e,in,&e->data.party_mons[e->party_slot].base,e->data.party_nicks[e->party_slot],e->data.party_ot[e->party_slot],1,&e->data.party_mons[e->party_slot],2);
}
static void boxes_tab(save_editor_t*e,unsigned in){
    if(!e->box_detail){box_list_tab(e,in);return;}
    char label[64];SDL_Rect b={42,112,150,30},s={210,112,150,30};int count=e->data.box_count[e->box_num];
    snprintf(label,sizeof(label),"BOX: %d OF %d",e->box_num+1,NUM_BOXES);
    LauncherDropdown_DrawField(e->r,b,label,e->focus==0,dropdown_owns_rect(b));
    snprintf(label,sizeof(label),"SLOT: %d",e->box_slot+1);
    LauncherDropdown_DrawField(e->r,s,label,e->focus==1,dropdown_owns_rect(s));
    if(count)snprintf(label,sizeof(label),"%d / %d STORED IN BOX %d",count,BOX_CAPACITY,e->box_num+1);else snprintf(label,sizeof(label),"BOX %d IS EMPTY",e->box_num+1);
    LauncherDraw_TextBold(e->r,390,LDRAW_TEXT_Y(112,30,1),1,LCOL_TEXT_DIM,label);
    if(control(e,in,0,b))open_workspace_dropdown(e,WDD_BOX,1,0,b,NUM_BOXES,e->box_num,box_label,NULL,NULL);
    SDL_Rect back={370,112,170,30};button(e,back,"BACK TO BOX LIST",e->focus==2);if(control(e,in,2,back)){e->box_detail=0;e->focus=0;return;}
    if(count==0){e->box_detail=0;e->focus=0;return;}
    if(e->box_slot>=count)e->box_slot=count-1;
    if(control(e,in,1,s))open_workspace_dropdown(e,WDD_BOX_SLOT,0,1,s,count,e->box_slot,slot_label,NULL,NULL);
    mon_form(e,in,&e->data.box_mons[e->box_num][e->box_slot],e->data.box_nicks[e->box_num][e->box_slot],e->data.box_ot[e->box_num][e->box_slot],0,NULL,3);
}

static void apply_dropdown_choice(save_editor_t *e, int choice) {
    if (choice < 0) return;
    if (workspace_dropdown_kind == WDD_SPECIES && workspace_dropdown_mon) {
        workspace_dropdown_mon->species = gDexToSpecies[choice + 1];
        e->dirty = 1;
    } else if (workspace_dropdown_kind == WDD_STATUS && workspace_dropdown_mon) {
        workspace_dropdown_mon->status = status_values[choice];
        e->dirty = 1;
    } else if (workspace_dropdown_kind == WDD_MOVE && workspace_dropdown_mon) {
        workspace_dropdown_mon->moves[workspace_dropdown_aux] = (uint8_t)choice;
        workspace_dropdown_mon->pp[workspace_dropdown_aux] = choice ? gMoves[choice].pp : 0;
        e->dirty = 1;
    } else if (workspace_dropdown_kind == WDD_PARTY_SLOT) {
        e->party_slot = choice;
    } else if (workspace_dropdown_kind == WDD_BOX) {
        e->box_num = choice;
        e->box_slot = 0;
    } else if (workspace_dropdown_kind == WDD_BOX_SLOT) {
        e->box_slot = choice;
    } else if (workspace_dropdown_kind == WDD_LOCATION) {
        if (SE_LocationApply(&e->data, choice)) {
            e->dirty = 1;
            snprintf(e->feedback, sizeof(e->feedback),
                     "LOCATION CHANGED; POSITION RESET");
            e->feedback_error = 0;
            e->feedback_until = SDL_GetTicks() + 2500;
        }
    }
}

int SE_Workspace(save_editor_t *e) {
    int running = 1;
    memset(&workspace_dropdown, 0, sizeof(workspace_dropdown));
    workspace_dropdown_kind = WDD_NONE;
    e->focus = 0;
    e->active_tab = 0;
    e->requested_tab = -1;
    while (running) {
        SE_UpdateCanvas(e);
        int quit = 0;
        unsigned in = SE_Poll(e, &quit);
        if (quit) break;

        tick_inline_edit(e,&in);

        if (workspace_dropdown.open) {
            LauncherDropdown_Text(&workspace_dropdown,e->dropdown_text,
                                  e->dropdown_backspace);
            if (workspace_dropdown.searchable)
                in = SE_FilterTextNavigation(in, e->dropdown_backspace);
            if (e->wheel) {
                LauncherDropdown_Wheel(&workspace_dropdown, e->wheel);
                in &= ~(LNAV_UP | LNAV_DOWN);
            }
            int picked = LauncherDropdown_Tick(&workspace_dropdown, e->nav, in);
            if (picked >= 0) apply_dropdown_choice(e, picked);
            if (picked != LDROP_NONE) workspace_dropdown_kind = WDD_NONE;
            e->requested_tab = -1;
            in = 0;
            e->nav->ptr_moved = 0;
            e->nav->ptr_pressed = 0;
            e->nav->ptr_released = 0;
        }

        if(e->save_requested){
            if(inline_edit.open)finish_inline_edit(e,1);
            if(workspace_dropdown.open){workspace_dropdown.open=0;if(workspace_dropdown.searchable)SDL_StopTextInput();workspace_dropdown_kind=WDD_NONE;}
            if(!e->dirty){snprintf(e->feedback,sizeof e->feedback,"NO CHANGES TO SAVE");e->feedback_error=0;}
            else if(Save_EditorWrite(e->path,&e->data)==0){e->dirty=0;e->data.location_changed=0;snprintf(e->feedback,sizeof e->feedback,"SAVE COMPLETE");e->feedback_error=0;}
            else{snprintf(e->feedback,sizeof e->feedback,"SAVE FAILED");e->feedback_error=1;}
            e->feedback_until=SDL_GetTicks()+2500;
            e->save_requested=0;in=0;e->nav->ptr_pressed=0;e->nav->ptr_released=0;
        }

        if(e->reload_requested){
            if(inline_edit.open)finish_inline_edit(e,0);
            if(workspace_dropdown.open){workspace_dropdown.open=0;if(workspace_dropdown.searchable)SDL_StopTextInput();workspace_dropdown_kind=WDD_NONE;}
            if(Save_EditorRead(e->path,&e->data)==0){e->dirty=0;snprintf(e->feedback,sizeof e->feedback,"SAVE RELOADED");e->feedback_error=0;}
            else{snprintf(e->feedback,sizeof e->feedback,"RELOAD FAILED");e->feedback_error=1;}
            e->feedback_until=SDL_GetTicks()+2500;
            e->reload_requested=0;in=0;e->nav->ptr_pressed=0;e->nav->ptr_released=0;
        }

        if (e->requested_tab >= 0) {
            e->active_tab = e->requested_tab;
            e->requested_tab = -1;
            e->focus = 0;
            e->scroll = 0;
            in &= ~LNAV_BACK;
        }
        if (in & (LNAV_BACK | LNAV_CANCEL)) break;

        SE_Header(e, e->active_tab == 0 ? "EVENTS" :
                     e->active_tab == 1 ? "PLAYER / BADGES / MONEY" :
                     e->active_tab == 2 ? "POKEDEX" :
                     e->active_tab == 3 ? "PARTY" : "PC BOXES");
        SDL_Rect panel = {24,100,LDRAW_W-48,LDRAW_H-142};
        LauncherDraw_Bevel(e->r,panel,0);
        if(e->active_tab==0)events_tab(e,in);
        else if(e->active_tab==1)player_tab(e,in);
        else if(e->active_tab==2)dex_tab(e,in);
        else if(e->active_tab==3)party_tab(e,in);
        else boxes_tab(e,in);

        LauncherDropdown_Draw(e->r, &workspace_dropdown);
        SE_Present(e,"EDIT","CLOSE");
    }
    if(inline_edit.open)finish_inline_edit(e,0);
    if (workspace_dropdown.open && workspace_dropdown.searchable) SDL_StopTextInput();
    workspace_dropdown.open = 0;
    workspace_dropdown_kind = WDD_NONE;
    return 0;
}
