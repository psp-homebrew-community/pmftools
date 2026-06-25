#include "mps_mux.h"
#include <stdlib.h>
#include <string.h>

#define PACK_HDR    14
#define SYS_HDR     18
#define PES_TS      19
#define PES_NOTS    9

static void w32be(uint8_t *p, uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void w16be(uint8_t *p, uint16_t v){p[0]=v>>8;p[1]=v;}

static int build_pack(uint8_t *o, uint64_t scr){
    uint32_t e=scr%300,b=scr/300;
    o[0]=0x00;o[1]=0x00;o[2]=0x01;o[3]=0xBA;
    o[4]=((b>>27)&7)|0x44;o[5]=(b>>19);o[6]=(((b>>15)&7)<<5)|4|((b>>12)&7);
    o[7]=(b>>5);o[8]=(((b>>3)&3)<<5)|4|((b>>1)&7);
    o[9]=((b&1)<<7)|((e>>7)&1)|6;o[10]=((e>>1)&0x7F)|2;o[11]=((e&1)<<7)|0x7E;
    o[12]=(PMF_MUX_RATE>>14);o[13]=(PMF_MUX_RATE>>6);
    return 14;
}

static int build_sys(uint8_t *o){
    int p=0;o[p++]=0x00;o[p++]=0x00;o[p++]=0x01;o[p++]=0xBB;
    int l=p;p+=2;o[p++]=0x80;o[p++]=0x00;o[p++]=0x00;o[p++]=0x04;o[p++]=0xE1;
    o[p++]=0x00;o[p++]=0x00;o[p++]=0xC0|0x3F;
    w16be(o+l,(uint16_t)(p-l-2));return p;
}

static int build_pes(uint8_t *o, uint8_t sid, int plen, uint64_t pts, uint64_t dts, int has_dts){
    int p=0;o[p++]=0x00;o[p++]=0x00;o[p++]=0x01;o[p++]=sid;
    int l=p;p+=2;int hp=(pts>0);int pdf=hp?(has_dts?3:2):0;
    o[p++]=(uint8_t)(0x80|(hp?0x80:0)|pdf);o[p++]=(uint8_t)(hp?(has_dts?10:5):0);
    if(hp){uint32_t pt=pts;
        o[p++]=((pdf<<4)|((pt>>30)&7)|0x20);o[p++]=(pt>>22);o[p++]=(((pt>>15)&0x7F)<<1|1);
        o[p++]=(pt>>7);o[p++]=(((pt&0x7F)<<1)|1);
        if(has_dts){uint32_t dt=dts;
            o[p++]=(0x10|((dt>>30)&7)|0x10);o[p++]=(dt>>22);o[p++]=(((dt>>15)&0x7F)<<1|1);
            o[p++]=(dt>>7);o[p++]=(((dt&0x7F)<<1)|1);}}
    w16be(o+l,(uint16_t)(plen+(p-l-2)));return p;
}

static int build_pad(uint8_t *o,int sz){
    if(sz<6)return 0;o[0]=0x00;o[1]=0x00;o[2]=0x01;o[3]=0xBE;
    w16be(o+4,sz-6);memset(o+6,0xFF,sz-6);return sz;
}

typedef struct{uint8_t*d;size_t s,c;}BV;
static void bv_init(BV*v,size_t cap){v->d=malloc(cap);v->s=0;v->c=cap;}
static void bv_add(BV*v,const uint8_t*d,size_t n){
    if(v->s+n>v->c){while(v->c<v->s+n)v->c*=2;v->d=realloc(v->d,v->c);}
    memcpy(v->d+v->s,d,n);v->s+=n;
}

MpsMuxResult mps_mux_build(MpsMuxInput *in){
    MpsMuxResult r={NULL,0};
    if(!in||!in->h264_data||!in->h264_size)return r;

    uint8_t sh[128],pb[64],pk[PMF_PACK_SIZE];
    int sl=build_sys(sh);
    uint64_t scr=PMF_SCR_INITIAL;
    uint64_t pd=((uint64_t)PMF_PACK_SIZE*PMF_CLOCK_27M)/PMF_MUX_RATE_DEN;

    size_t off=0;int si=0;
    BV out;bv_init(&out,in->h264_size+65536);

    while(off<in->h264_size){
        int do_ts=(si>=15)||(off==0);
        uint64_t pts=PMF_PTS_VIDEO_START+(uint64_t)off/PMF_FRAME_DURATION_TICKS*PMF_FRAME_DURATION_TICKS;
        uint64_t dts=pts-PMF_FRAME_DURATION_TICKS;
        int rem=in->h264_size-off;
        int over=do_ts?PES_TS:PES_NOTS;
        int maxp=PMF_PACK_SIZE-PACK_HDR-sl-over-6;
        if(maxp<64)maxp=64;
        int ch=rem<maxp?rem:maxp;
        int pl=build_pes(pb,SID_VIDEO,ch,do_ts?pts:0,do_ts?dts:0,do_ts);
        int used=PACK_HDR+sl+pl+ch;
        int pad=PMF_PACK_SIZE-used;

        int pos=build_pack(pk,scr);
        memcpy(pk+pos,sh,sl);pos+=sl;
        memcpy(pk+pos,pb,pl);pos+=pl;
        memcpy(pk+pos,in->h264_data+off,ch);pos+=ch;
        if(pad>=6)pos+=build_pad(pk+pos,pad);
        bv_add(&out,pk,PMF_PACK_SIZE);

        off+=ch;si=do_ts?0:si+1;scr+=pd;
    }

    r.data=out.d;r.size=out.s;return r;
}

void mps_mux_result_free(MpsMuxResult*r){if(r&&r->data){free(r->data);r->data=NULL;r->size=0;}}
