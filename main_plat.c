/* PLATFORMER 32 - PS2SDK + gsKit
 * 32 fases. Segredo: no TITULO segure CIMA + SELECT + X(A) -> flash branco,
 * flag salva no memory card (mc0:PLAT32/SECRET). Reset, abra o jogo, zere
 * as 32 fases -> cutscene do poema. A flag e apagada depois da cutscene.
 */
#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <fileio.h>
#include <libpad.h>
#include <libmc.h>
#include <gsKit.h>
#include <dmaKit.h>
#include "poem.h"

#define NUM_LEVELS 32
#define MAXP 40
#define SECRET_DIR  "mc0:PLAT32"
#define SECRET_FILE "mc0:PLAT32/SECRET"

typedef struct { float x, y, w; } Plat;
typedef enum { S_TITLE, S_PLAY, S_CUTSCENE, S_END } State;

static GSGLOBAL *gs;
static GSFONTM *font;
static char padBuf[256] __attribute__((aligned(64)));
static u32 btn, prev;
#define PRESSED(b) ((btn & (b)) && !(prev & (b)))

static Plat plat[MAXP];
static int np, level = 1, secretArmed = 0, flash = 0;
static float px, py, vx, vy, cam;
static int onGround;
static State state = S_TITLE;
static int cutT = 0;

#define RGB(r,g,b) GS_SETREG_RGBAQ(r,g,b,0x80,0)

static void rect(float x1, float y1, float x2, float y2, u64 c) {
    gsKit_prim_sprite(gs, x1, y1, x2, y2, 1, c);
}
static void text(float x, float y, float s, u64 c, const char *t) {
    gsKit_fontm_print_scaled(gs, font, x, y, 2, s, c, t);
}

/* ---------- memory card flag ---------- */
static void saveSecret(int on) {
    if (on) {
        int fd;
        fioMkdir(SECRET_DIR);
        fd = fioOpen(SECRET_FILE, FIO_O_WRONLY | FIO_O_CREAT | FIO_O_TRUNC);
        if (fd >= 0) { fioWrite(fd, "1", 1); fioClose(fd); }
    } else {
        fioRemove(SECRET_FILE);
    }
}
static int loadSecret(void) {
    int fd = fioOpen(SECRET_FILE, FIO_O_RDONLY);
    if (fd < 0) return 0;
    fioClose(fd);
    return 1;
}

/* ---------- input ---------- */
static void readPad(void) {
    struct padButtonStatus b;
    prev = btn;
    if (padRead(0, 0, &b)) btn = 0xffff ^ b.btns; else btn = 0;
}

/* ---------- levels ---------- */
static void genLevel(int lv) {
    int i, n = 8 + lv / 2;
    srand(lv * 7919 + 13);
    if (n > MAXP) n = MAXP;
    np = 0;
    plat[np++] = (Plat){0, 360, 220};
    for (i = 1; i < n; i++) {
        Plat *p = &plat[np - 1], q;
        float gap = 40 + lv * 2 + rand() % 40;
        if (gap > 110) gap = 110;
        q.x = p->x + p->w + gap;
        q.y = p->y + (rand() % 100) - 50;
        if (q.y < 220) q.y = 220;
        if (q.y > 400) q.y = 400;
        q.w = 150 - lv * 2 - rand() % 30;
        if (q.w < 50) q.w = 50;
        plat[np++] = q;
    }
    px = 40; py = 300; vx = vy = 0; cam = 0;
}

static void updatePlay(void) {
    float oldFeet = py + 24, newFeet;
    int i;
    vx = 0;
    if (btn & PAD_LEFT)  vx = -4;
    if (btn & PAD_RIGHT) vx =  4;
    if (PRESSED(PAD_CROSS) && onGround) { vy = -11; onGround = 0; }
    vy += 0.55f;
    px += vx; py += vy;
    if (px < 0) px = 0;
    newFeet = py + 24;
    onGround = 0;
    if (vy >= 0) {
        for (i = 0; i < np; i++) {
            if (px + 16 > plat[i].x && px < plat[i].x + plat[i].w &&
                oldFeet <= plat[i].y + 1 && newFeet >= plat[i].y) {
                py = plat[i].y - 24; vy = 0; onGround = 1; break;
            }
        }
    }
    cam = px - 200;
    if (cam < 0) cam = 0;
    if (py > 600) genLevel(level);               /* caiu: reinicia fase */
    {   /* chegou na ultima plataforma */
        Plat *g = &plat[np - 1];
        if (onGround && px > g->x + g->w / 2 - 16 && px < g->x + g->w) {
            if (++level > NUM_LEVELS) {
                if (secretArmed) { state = S_CUTSCENE; cutT = 0; }
                else state = S_END;
            } else genLevel(level);
        }
    }
}

static void drawPlay(void) {
    char buf[32];
    int i;
    for (i = 0; i < np; i++)
        rect(plat[i].x - cam, plat[i].y, plat[i].x + plat[i].w - cam, plat[i].y + 12, RGB(80, 200, 80));
    {   Plat *g = &plat[np - 1];
        float fx = g->x + g->w / 2 - cam;
        rect(fx, g->y - 40, fx + 4, g->y, RGB(255, 255, 255));
        rect(fx + 4, g->y - 40, fx + 20, g->y - 28, RGB(255, 60, 60));
    }
    rect(px - cam, py, px - cam + 16, py + 24, RGB(240, 200, 40));
    snprintf(buf, sizeof buf, "FASE %d/%d", level, NUM_LEVELS);
    text(20, 20, 0.7f, RGB(255, 255, 255), buf);
}

/* ---------- cutscene ---------- */
static void drawCutscene(void) {
    static const char *intro[] = {
        "ALGUEM ESCREVE A LUZ DE UMA VELA...", ""
    };
    static const char *outro[] = {
        "", "ISTO E PARA OUTRA PESSOA.", "EU NAO VOU DIZER QUEM."
    };
    int total = 0, shown, y = 70, i, k;
    int nIntro = 2, nOut = 3;
    const char *lines[16];
    int n = 0;
    for (i = 0; i < nIntro; i++) lines[n++] = intro[i];
    for (i = 0; i < POEM_LINES; i++) lines[n++] = POEM[i];
    for (i = 0; i < nOut; i++) lines[n++] = outro[i];

    shown = cutT / 4;                      /* maquina de escrever */
    for (k = 0; k < n; k++) {
        int len = strlen(lines[k]);
        int take = shown - total;
        char buf[96];
        if (take > len) take = len;
        if (take > 0) {
            memcpy(buf, lines[k], take); buf[take] = 0;
            text(40, y, 0.8f, RGB(255, 255, 255), buf);
        }
        total += len + 10;                 /* pausa entre linhas */
        y += 32;
    }
    /* "vela" piscando */
    if ((cutT / 15) & 1) rect(580, 380, 586, 392, RGB(255, 180, 40));
    rect(576, 392, 590, 430, RGB(220, 220, 200));
}

/* ---------- main ---------- */
int main(void) {
    SifInitRpc(0);
    while (!SifIopReset("", 0)) {}
    while (!SifIopSync()) {}
    SifInitRpc(0);
    SifLoadFileInit();
    fioInit();
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    SifLoadModule("rom0:MCMAN", 0, NULL);
    SifLoadModule("rom0:MCSERV", 0, NULL);
    mcInit(MC_TYPE_MC);
    padInit(0);
    padPortOpen(0, 0, padBuf);
    while (padGetState(0, 0) != PAD_STATE_STABLE &&
           padGetState(0, 0) != PAD_STATE_FINDCTP1) {}

    gs = gsKit_init_global();
    gs->ZBuffering = GS_SETTING_OFF;
    gs->PrimAlphaEnable = GS_SETTING_OFF;
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    font = gsKit_init_fontm();
    gsKit_fontm_upload(gs, font);
    font->Spacing = 0.8f;

    secretArmed = loadSecret();

    for (;;) {
        readPad();
        gsKit_clear(gs, RGB(0, 0, 0));
        switch (state) {
        case S_TITLE:
            text(150, 130, 1.2f, RGB(255, 255, 255), "PLATFORMER 32");
            text(200, 260, 0.7f, RGB(200, 200, 200), "APERTE START");
            if ((btn & PAD_UP) && (btn & PAD_SELECT) && (btn & PAD_CROSS)
                && !secretArmed) {
                secretArmed = 1; saveSecret(1); flash = 20;
            }
            if (PRESSED(PAD_START)) { level = 1; genLevel(1); state = S_PLAY; }
            break;
        case S_PLAY:
            updatePlay();
            if (state == S_PLAY) drawPlay();
            break;
        case S_CUTSCENE:
            drawCutscene();
            cutT++;
            if (cutT > 60 * 45 || (cutT > 600 && PRESSED(PAD_START))) {
                saveSecret(0); secretArmed = 0; state = S_END;
            }
            break;
        case S_END:
            text(170, 190, 1.0f, RGB(255, 255, 255), "FIM");
            if (PRESSED(PAD_START)) state = S_TITLE;
            break;
        }
        if (flash > 0) {
            rect(0, 0, gs->Width, gs->Height, RGB(255, 255, 255));
            flash--;
        }
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }
    return 0;
}
