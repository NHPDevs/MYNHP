#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define NHP_VERSION "3.0"
#define MAX_TOK  16384
#define MAX_STR  1024
#define MAX_ID   256
#define MAX_CH   64
#define MAX_VARS 100000

typedef enum { E_WARN, E_ERROR, E_FATAL } ELevel;

typedef struct {
    int line, col;
    char msg[512];
    ELevel level;
} NHPError;

static NHPError g_errors[256];
static int g_nerrors = 0;

static void err_push(ELevel lv, int line, int col, const char *fmt, ...) {
    if(g_nerrors >= 256) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_errors[g_nerrors].msg, 511, fmt, ap);
    va_end(ap);
    g_errors[g_nerrors].line = line;
    g_errors[g_nerrors].col = col;
    g_errors[g_nerrors].level = lv;
    g_nerrors++;
}

static void err_print_all(void) {
    int i;
    for(i = 0; i < g_nerrors; i++) {
        NHPError *e = &g_errors[i];
        const char *lv = e->level == E_WARN ? "WARNING" : e->level == E_ERROR ? "ERROR" : "FATAL";
        fprintf(stderr, "[NHP %s] line %d: %s\n", lv, e->line, e->msg);
    }
    if(g_nerrors > 0) {
        int errs = 0, warns = 0;
        for(i = 0; i < g_nerrors; i++) {
            if(g_errors[i].level >= E_ERROR) errs++;
            else warns++;
        }
        if(errs) fprintf(stderr, "[NHP] %d error(s), %d warning(s)\n", errs, warns);
    }
    g_nerrors = 0;
}

typedef enum {
    TK_PSET, TK_BYTES, TK_LEN, TK_BEGIN, TK_DOLLAR,
    TK_MATHS, TK_REG, TK_REMP, TK_EXECUTE,
    TK_STORAGE, TK_LOAD, TK_LLTD, TK_PARSE, TK_MT, TK_IS, TK_IMR,
    TK_IMP, TK_ADD, TK_SUB, TK_MUL, TK_DIV,
    TK_CLS, TK_JNS, TK_PRINT, TK_ALIAS,
    TK_SCR, TK_SCT, TK_SCX, TK_SCZ, TK_SCN,
    TK_BTN, TK_TEXTBOX, TK_OUTPUT_KW,
    TK_MOUSE_CLICK, TK_CUSTOMER_NAME,
    TK_VAR, TK_MYVAR, TK_IF, TK_ELSE, TK_FOR, TK_WHILE,
    TK_INPUT, TK_INPUTS, TK_RAT, TK_MSG, TK_REN, TK_STC,
    TK_GT, TK_LT, TK_GTE, TK_LTE, TK_NEQ, TK_EQEQ,
    TK_IDENT, TK_NUMBER, TK_STRING, TK_CHAR, TK_LABEL,
    TK_LBR, TK_RBR, TK_LP, TK_RP, TK_LB, TK_RB,
    TK_COMMA, TK_EQ, TK_MINUS, TK_STAR, TK_PLUS,
    TK_COLON, TK_SEMI, TK_PERCENT,
    TK_AMPERSAND, TK_TOK, TK_ASM_BLOCK,
    TK_EOF, TK_UNKNOWN
} TK;

typedef struct {
    TK type;
    char val[MAX_STR];
    int line, col, ival;
    double fval;
} Token;

typedef struct { const char *w; TK t; } KW;

static KW KWS[] = {
    {"Pset", TK_PSET}, {"Maths", TK_MATHS}, {"Reg", TK_REG}, {"Remp", TK_REMP},
    {"Execute_system", TK_EXECUTE},
    {"IMP", TK_IMP}, {"Add", TK_ADD}, {"Sub", TK_SUB}, {"Mul", TK_MUL}, {"Div", TK_DIV},
    {"cls", TK_CLS}, {"jns", TK_JNS}, {"Print", TK_PRINT}, {"print", TK_PRINT},
    {"Alias", TK_ALIAS},
    {"SCR", TK_SCR}, {"SCT", TK_SCT}, {"SCX", TK_SCX}, {"SCZ", TK_SCZ}, {"SCN", TK_SCN},
    {"Textbox", TK_TEXTBOX}, {"Output", TK_OUTPUT_KW},
    {"Mouse2Click", TK_MOUSE_CLICK}, {"CustomerName", TK_CUSTOMER_NAME},
    {"LP", TK_LOAD}, {"LLtd", TK_LLTD}, {"Parse", TK_PARSE},
    {"IS", TK_IS}, {"IMR", TK_IMR}, {"MT", TK_MT}, {"mt", TK_MT},
    {"Tok", TK_TOK},
    {"var", TK_VAR}, {"My", TK_MYVAR}, {"my", TK_MYVAR},
    {"if", TK_IF}, {"If", TK_IF}, {"IF", TK_IF},
    {"else", TK_ELSE}, {"Else", TK_ELSE},
    {"For", TK_FOR}, {"for", TK_FOR},
    {"while", TK_WHILE},
    {"InpuS", TK_INPUTS}, {"Rat", TK_RAT}, {"msg", TK_MSG},
    {"ren", TK_REN}, {"stc", TK_STC},
    {NULL, TK_UNKNOWN}
};

typedef struct {
    char name[MAX_ID];
    int type, used, scope;
    union {
        int ival;
        double fval;
        char *sval;
    } value;
} Variable;

static Variable *g_vars = NULL;
static int g_var_count = 0;
static int g_var_capacity = 0;

static void var_init(void) {
    g_var_capacity = 1024;
    g_vars = (Variable*)calloc(g_var_capacity, sizeof(Variable));
}

static void var_free(void) {
    int i;
    for(i = 0; i < g_var_count; i++) {
        if(g_vars[i].type == 2 && g_vars[i].value.sval) {
            free(g_vars[i].value.sval);
        }
    }
    free(g_vars);
}

static int var_add(const char *name, int type) {
    if(g_var_count >= g_var_capacity) {
        g_var_capacity *= 2;
        g_vars = (Variable*)realloc(g_vars, g_var_capacity * sizeof(Variable));
    }
    strncpy(g_vars[g_var_count].name, name, MAX_ID - 1);
    g_vars[g_var_count].type = type;
    g_vars[g_var_count].used = 0;
    g_vars[g_var_count].scope = 0;
    return g_var_count++;
}

static int var_find(const char *name) {
    int i;
    for(i = g_var_count - 1; i >= 0; i--) {
        if(strcmp(g_vars[i].name, name) == 0) {
            g_vars[i].used = 1;
            return i;
        }
    }
    return -1;
}

typedef struct {
    char *buf;
    int len, cap;
} OB;

static void ob_init(OB *b) {
    b->cap = 65536;
    b->buf = (char*)malloc(b->cap);
    b->buf[0] = 0;
    b->len = 0;
}

static void ob_free(OB *b) {
    free(b->buf);
    b->buf = NULL;
}

static void ob_grow(OB *b, int n) {
    while(b->len + n + 4 >= b->cap) {
        b->cap *= 2;
        b->buf = (char*)realloc(b->buf, b->cap);
    }
}

static void ob_w(OB *b, const char *fmt, ...) {
    va_list ap;
    ob_grow(b, 1024);
    va_start(ap, fmt);
    int n = vsnprintf(b->buf + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    if(n > 0 && b->len + n < b->cap) b->len += n;
}

static void ob_l(OB *b, const char *fmt, ...) {
    va_list ap;
    ob_grow(b, 1024);
    va_start(ap, fmt);
    int n = vsnprintf(b->buf + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    if(n > 0 && b->len + n < b->cap) {
        b->len += n;
        b->buf[b->len++] = '\n';
        b->buf[b->len] = 0;
    }
}

typedef struct {
    const char *src;
    int pos, line, col;
    Token *t;
    int cnt, cap;
} LX;

static void lx_init(LX *l, const char *s) {
    l->src = s;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
    l->cap = 1024;
    l->cnt = 0;
    l->t = (Token*)malloc(l->cap * sizeof(Token));
}

static void lx_free(LX *l) {
    free(l->t);
}

static char lp(LX *l) {
    return l->src[l->pos];
}

static char lp2(LX *l) {
    return l->src[l->pos] ? l->src[l->pos + 1] : 0;
}

static char la(LX *l) {
    char c = l->src[l->pos++];
    if(c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

static void lpush(LX *l, TK t, const char *v, int ln, int col) {
    if(l->cnt >= l->cap) {
        l->cap *= 2;
        l->t = (Token*)realloc(l->t, l->cap * sizeof(Token));
    }
    l->t[l->cnt].type = t;
    l->t[l->cnt].line = ln;
    l->t[l->cnt].col = col;
    strncpy(l->t[l->cnt].val, v ? v : "", MAX_STR - 1);
    l->t[l->cnt].ival = 0;
    l->t[l->cnt].fval = 0.0;
    if(t == TK_NUMBER && v) {
        l->t[l->cnt].ival = (int)strtol(v, NULL, 0);
        l->t[l->cnt].fval = strtod(v, NULL);
    }
    l->cnt++;
}

static void skip_line_comment(LX *l) {
    while(lp(l) && lp(l) != '\n') la(l);
}

static void skip_block_comment(LX *l) {
    la(l); la(l);
    while(lp(l)) {
        if(lp(l) == '*' && lp2(l) == '/') {
            la(l); la(l);
            break;
        }
        la(l);
    }
}

static void lx_run(LX *l) {
    while(lp(l)) {
        while(lp(l) && (lp(l) == ' ' || lp(l) == '\t' || lp(l) == '\r' || lp(l) == '\n'))
            la(l);
        
        if(!lp(l)) break;
        
        int ln = l->line, co = l->col;
        char c = lp(l);
        const char *s = &l->src[l->pos];
        
        if(c == ';' || (c == '/' && lp2(l) == '/')) {
            skip_line_comment(l);
            continue;
        }
        
        if(c == '/' && lp2(l) == '*') {
            skip_block_comment(l);
            continue;
        }
        
        if(c == '>' && lp2(l) == '=') {
            la(l); la(l);
            lpush(l, TK_GTE, ">=", ln, co);
            continue;
        }
        
        if(c == '<' && lp2(l) == '=') {
            la(l); la(l);
            lpush(l, TK_LTE, "<=", ln, co);
            continue;
        }
        
        if(c == '!' && lp2(l) == '=') {
            la(l); la(l);
            lpush(l, TK_NEQ, "!=", ln, co);
            continue;
        }
        
        if(c == '=' && lp2(l) == '=') {
            la(l); la(l);
            lpush(l, TK_EQEQ, "==", ln, co);
            continue;
        }
        
        if(strncmp(s, "I*Storage", 9) == 0) {
            int i; for(i = 0; i < 9; i++) la(l);
            lpush(l, TK_STORAGE, "I*Storage", ln, co);
            continue;
        }
        
        if(strncmp(s, "InpuS", 5) == 0) {
            int i; for(i = 0; i < 5; i++) la(l);
            lpush(l, TK_INPUTS, "InpuS", ln, co);
            continue;
        }
        
        if(strncmp(s, "====Asm", 7) == 0) {
            int i; for(i = 0; i < 7; i++) la(l);
            lpush(l, TK_ASM_BLOCK, "====Asm", ln, co);
            while(lp(l) && lp(l) != '{') la(l);
            if(!lp(l)) continue;
            la(l);
            int cap = 4096;
            char *buf = (char*)malloc(cap);
            int bi = 0, depth = 1;
            while(lp(l) && depth > 0) {
                char cc = lp(l);
                if(cc == '{') depth++;
                if(cc == '}') {
                    depth--;
                    if(depth == 0) {
                        la(l);
                        break;
                    }
                }
                if(bi >= cap - 2) {
                    cap *= 2;
                    buf = (char*)realloc(buf, cap);
                }
                buf[bi++] = la(l);
            }
            buf[bi] = 0;
            lpush(l, TK_ASM_BLOCK, buf, ln, co);
            free(buf);
            continue;
        }
        
        if(c == '"') {
            la(l);
            char buf[MAX_STR] = {0};
            int bi = 0;
            while(lp(l) && lp(l) != '"') {
                if(lp(l) == '\\') {
                    la(l);
                    char e = la(l);
                    switch(e) {
                        case 'n': buf[bi++] = '\n'; break;
                        case 't': buf[bi++] = '\t'; break;
                        case 'r': buf[bi++] = '\r'; break;
                        case '\\': buf[bi++] = '\\'; break;
                        case '"': buf[bi++] = '"'; break;
                        default: if(bi < MAX_STR - 1) buf[bi++] = e;
                    }
                } else {
                    if(bi < MAX_STR - 1) buf[bi++] = la(l);
                    else la(l);
                }
            }
            if(lp(l) == '"') la(l);
            lpush(l, TK_STRING, buf, ln, co);
            continue;
        }
        
        if(c == '\'') {
            la(l);
            char buf[4] = {0};
            if(lp(l) == '\\') {
                la(l);
                char e = la(l);
                switch(e) {
                    case 'n': buf[0] = '\n'; break;
                    case 't': buf[0] = '\t'; break;
                    default: buf[0] = e;
                }
            } else {
                buf[0] = la(l);
            }
            if(lp(l) == '\'') la(l);
            lpush(l, TK_CHAR, buf, ln, co);
            continue;
        }
        
        if(isdigit((unsigned char)c) || (c == '0' && (lp2(l) == 'x' || lp2(l) == 'X'))) {
            char buf[128] = {0};
            int bi = 0;
            if(c == '0' && (lp2(l) == 'x' || lp2(l) == 'X')) {
                buf[bi++] = la(l);
                buf[bi++] = la(l);
                while(isxdigit((unsigned char)lp(l)) && bi < 126)
                    buf[bi++] = la(l);
            } else {
                while((isdigit((unsigned char)lp(l)) || lp(l) == '.') && bi < 126)
                    buf[bi++] = la(l);
            }
            lpush(l, TK_NUMBER, buf, ln, co);
            continue;
        }
        
        if(isalpha((unsigned char)c) || c == '_') {
            char buf[MAX_ID] = {0};
            int bi = 0;
            while(lp(l) && (isalnum((unsigned char)lp(l)) || lp(l) == '_' || lp(l) == '.') && bi < MAX_ID - 1)
                buf[bi++] = la(l);
            
            if(lp(l) == ':') {
                la(l);
                lpush(l, TK_LABEL, buf, ln, co);
                continue;
            }
            
            int k, found = 0;
            for(k = 0; KWS[k].w; k++) {
                if(strcmp(buf, KWS[k].w) == 0) {
                    lpush(l, KWS[k].t, buf, ln, co);
                    found = 1;
                    break;
                }
            }
            if(!found) {
                lpush(l, TK_IDENT, buf, ln, co);
            }
            continue;
        }
        
        la(l);
        switch(c) {
            case '$': lpush(l, TK_DOLLAR, "$", ln, co); break;
            case '[': lpush(l, TK_LBR, "[", ln, co); break;
            case ']': lpush(l, TK_RBR, "]", ln, co); break;
            case '(': lpush(l, TK_LP, "(", ln, co); break;
            case ')': lpush(l, TK_RP, ")", ln, co); break;
            case '{': lpush(l, TK_LB, "{", ln, co); break;
            case '}': lpush(l, TK_RB, "}", ln, co); break;
            case ',': lpush(l, TK_COMMA, ",", ln, co); break;
            case '=': lpush(l, TK_EQ, "=", ln, co); break;
            case '+': lpush(l, TK_PLUS, "+", ln, co); break;
            case '-': lpush(l, TK_MINUS, "-", ln, co); break;
            case '*': lpush(l, TK_STAR, "*", ln, co); break;
            case '%': lpush(l, TK_PERCENT, "%", ln, co); break;
            case '&': lpush(l, TK_AMPERSAND, "&", ln, co); break;
            case ';': lpush(l, TK_SEMI, ";", ln, co); break;
            case ':': lpush(l, TK_COLON, ":", ln, co); break;
            case '>': lpush(l, TK_GT, ">", ln, co); break;
            case '<': lpush(l, TK_LT, "<", ln, co); break;
            default: break;
        }
    }
    lpush(l, TK_EOF, "", l->line, l->col);
}

typedef enum {
    N_PROG, N_PSET, N_BYTES, N_MATHS, N_REG, N_EXEC,
    N_IMP, N_ADD, N_SUB, N_MUL, N_DIV, N_CLS, N_JNS,
    N_LABEL, N_PRINT, N_ALIAS, N_BTN, N_TEXTBOX, N_OUTPUT,
    N_CLICK, N_CUSTNAME, N_ASM,
    N_STORAGE, N_LOAD, N_PARSE, N_MT, N_IS, N_IMR, N_TOK,
    N_VAR_DECL, N_VAR_ASSIGN, N_IF, N_ELSE, N_FOR, N_WHILE,
    N_INPUT, N_CONDITION, N_BLOCK,
    N_NUM, N_STR, N_IDENT, N_REG2, N_EXPR
} NT;

typedef struct AN {
    NT type;
    char v[MAX_STR], v2[MAX_STR], v3[MAX_STR];
    int ival, line, nch;
    double fval;
    struct AN *ch[MAX_CH];
} AN;

static AN *an(NT t, int l) {
    AN *n = (AN*)calloc(1, sizeof(AN));
    n->type = t;
    n->line = l;
    return n;
}

static void ac(AN *p, AN *c) {
    if(p && c && p->nch < MAX_CH)
        p->ch[p->nch++] = c;
}

static void afree(AN *n) {
    if(!n) return;
    int i;
    for(i = 0; i < n->nch; i++)
        afree(n->ch[i]);
    free(n);
}

typedef struct {
    Token *t;
    int pos, cnt;
} PR;

static Token *pc(PR *p) {
    return &p->t[p->pos];
}

static Token *pe(PR *p) {
    return &p->t[p->pos++];
}

static int pa(PR *p, TK t) {
    return pc(p)->type == t;
}

static void pm(PR *p, TK t) {
    if(pa(p, t)) pe(p);
}

static AN *parse_stmt(PR *p);

static AN *pop2(PR *p) {
    AN *n = NULL;
    int ln = pc(p)->line;
    switch(pc(p)->type) {
        case TK_SCR: case TK_SCT: case TK_SCX: case TK_SCZ: case TK_SCN:
            n = an(N_REG2, ln);
            strcpy(n->v, pe(p)->val);
            break;
        case TK_NUMBER: {
            Token *t = pe(p);
            n = an(N_NUM, ln);
            strcpy(n->v, t->val);
            n->ival = t->ival;
            n->fval = t->fval;
            break;
        }
        case TK_CHAR: {
            Token *t = pe(p);
            n = an(N_NUM, ln);
            n->ival = (unsigned char)t->val[0];
            snprintf(n->v, MAX_STR, "%d", n->ival);
            break;
        }
        case TK_STRING:
            n = an(N_STR, ln);
            strcpy(n->v, pe(p)->val);
            break;
        case TK_IDENT: case TK_LABEL:
            n = an(N_IDENT, ln);
            strcpy(n->v, pe(p)->val);
            break;
        case TK_MINUS:
            pe(p);
            n = an(N_NUM, ln);
            if(pa(p, TK_NUMBER)) {
                Token *t = pe(p);
                n->ival = -t->ival;
                n->fval = -t->fval;
                snprintf(n->v, MAX_STR, "%d", n->ival);
            }
            break;
        case TK_AMPERSAND:
            pe(p);
            if(pa(p, TK_IDENT)) {
                n = an(N_IDENT, ln);
                strcpy(n->v, pe(p)->val);
                strcpy(n->v2, "ref");
            }
            break;
        default:
            pe(p);
            break;
    }
    return n;
}

static AN *p2op(PR *p, NT nt) {
    int ln = pc(p)->line;
    pe(p);
    AN *n = an(nt, ln);
    AN *a = pop2(p);
    if(a) ac(n, a);
    pm(p, TK_COMMA);
    AN *b = pop2(p);
    if(b) ac(n, b);
    return n;
}

static AN *parse_condition(PR *p) {
    AN *n = an(N_CONDITION, pc(p)->line);
    AN *left = pop2(p);
    if(left) ac(n, left);
    TK op = pc(p)->type;
    strcpy(n->v, pc(p)->val);
    if(op == TK_GT || op == TK_LT || op == TK_GTE || op == TK_LTE || op == TK_EQEQ || op == TK_NEQ) {
        pe(p);
    }
    AN *right = pop2(p);
    if(right) ac(n, right);
    return n;
}

static AN *parse_stmt(PR *p) {
    int ln = pc(p)->line;
    switch(pc(p)->type) {
        case TK_IMP: return p2op(p, N_IMP);
        case TK_ADD: return p2op(p, N_ADD);
        case TK_SUB: return p2op(p, N_SUB);
        case TK_MUL: return p2op(p, N_MUL);
        case TK_DIV: return p2op(p, N_DIV);
        case TK_CLS: return p2op(p, N_CLS);
        case TK_JNS: {
            pe(p);
            AN *n = an(N_JNS, ln);
            if(pa(p, TK_IDENT) || pa(p, TK_LABEL))
                strcpy(n->v, pe(p)->val);
            return n;
        }
        case TK_PRINT: {
            pe(p);
            AN *n = an(N_PRINT, ln);
            pm(p, TK_LP);
            if(pa(p, TK_STRING)) {
                strcpy(n->v, pe(p)->val);
            }
            while(pa(p, TK_COMMA)) {
                pe(p);
                AN *arg = pop2(p);
                if(arg) ac(n, arg);
            }
            pm(p, TK_RP);
            return n;
        }
        case TK_BEGIN: case TK_LABEL: {
            AN *n = an(N_LABEL, ln);
            strcpy(n->v, pe(p)->val);
            return n;
        }
        case TK_MYVAR: {
            pe(p);
            AN *n = an(N_VAR_DECL, ln);
            if(pa(p, TK_IDENT)) {
                strcpy(n->v, pe(p)->val);
                var_add(n->v, 0);
            }
            return n;
        }
        case TK_INPUTS: {
            pe(p);
            AN *n = an(N_INPUT, ln);
            pm(p, TK_LP);
            if(pa(p, TK_STRING)) {
                strcpy(n->v, pe(p)->val);
            }
            pm(p, TK_COMMA);
            AN *var = pop2(p);
            if(var) ac(n, var);
            pm(p, TK_RP);
            return n;
        }
        case TK_IF: {
            pe(p);
            AN *n = an(N_IF, ln);
            AN *cond = parse_condition(p);
            if(cond) ac(n, cond);
            pm(p, TK_LB);
            AN *block = an(N_BLOCK, ln);
            while(!pa(p, TK_RB) && !pa(p, TK_EOF)) {
                AN *stmt = parse_stmt(p);
                if(stmt) ac(block, stmt);
            }
            ac(n, block);
            pm(p, TK_RB);
            if(pa(p, TK_ELSE)) {
                pe(p);
                pm(p, TK_LB);
                AN *else_block = an(N_BLOCK, ln);
                while(!pa(p, TK_RB) && !pa(p, TK_EOF)) {
                    AN *stmt = parse_stmt(p);
                    if(stmt) ac(else_block, stmt);
                }
                ac(n, else_block);
                pm(p, TK_RB);
            }
            return n;
        }
        case TK_FOR: {
            pe(p);
            AN *n = an(N_FOR, ln);
            pm(p, TK_LP);
            while(!pa(p, TK_RP) && !pa(p, TK_EOF)) {
                pe(p);
            }
            pm(p, TK_RP);
            pm(p, TK_LB);
            AN *block = an(N_BLOCK, ln);
            while(!pa(p, TK_RB) && !pa(p, TK_EOF)) {
                AN *stmt = parse_stmt(p);
                if(stmt) ac(block, stmt);
            }
            ac(n, block);
            pm(p, TK_RB);
            return n;
        }
        case TK_ASM_BLOCK: {
            pe(p);
            AN *n = an(N_ASM, ln);
            if(pa(p, TK_ASM_BLOCK))
                strcpy(n->v, pe(p)->val);
            return n;
        }
        default:
            pe(p);
            return NULL;
    }
}

static AN *parse_prog(LX *lx) {
    PR p;
    p.t = lx->t;
    p.cnt = lx->cnt;
    p.pos = 0;
    AN *root = an(N_PROG, 0);
    strcpy(root->v, "program");
    while(!pa(&p, TK_EOF)) {
        AN *n = NULL;
        int ln = pc(&p)->line;
        switch(pc(&p)->type) {
            case TK_PSET: {
                pe(&p);
                n = an(N_PSET, ln);
                if(pa(&p, TK_STRING))
                    strcpy(n->v, pe(&p)->val);
                else if(pa(&p, TK_IDENT))
                    strcpy(n->v, pe(&p)->val);
                break;
            }
            case TK_MATHS: {
                pe(&p);
                n = an(N_MATHS, ln);
                pm(&p, TK_LB);
                while(!pa(&p, TK_RB) && !pa(&p, TK_EOF)) {
                    AN *stmt = parse_stmt(&p);
                    if(stmt) ac(n, stmt);
                }
                pm(&p, TK_RB);
                break;
            }
            case TK_REG: {
                pe(&p);
                n = an(N_REG, ln);
                pm(&p, TK_LB);
                while(!pa(&p, TK_RB) && !pa(&p, TK_EOF)) {
                    AN *stmt = parse_stmt(&p);
                    if(stmt) ac(n, stmt);
                }
                pm(&p, TK_RB);
                break;
            }
            case TK_EXECUTE: {
                pe(&p);
                n = an(N_EXEC, ln);
                pm(&p, TK_LB);
                while(!pa(&p, TK_RB) && !pa(&p, TK_EOF)) {
                    AN *stmt = parse_stmt(&p);
                    if(stmt) ac(n, stmt);
                }
                pm(&p, TK_RB);
                break;
            }
            default:
                n = parse_stmt(&p);
                break;
        }
        if(n) ac(root, n);
    }
    return root;
}

static const char *r64(const char *r) {
    if(!strcmp(r, "SCR")) return "rax";
    if(!strcmp(r, "SCT")) return "rbx";
    if(!strcmp(r, "SCX")) return "rcx";
    if(!strcmp(r, "SCZ")) return "rdx";
    if(!strcmp(r, "SCN")) return "r8";
    return r;
}

static void opstr(AN *op, char *buf, int sz) {
    if(!op) {
        buf[0] = 0;
        return;
    }
    switch(op->type) {
        case N_REG2:
            snprintf(buf, sz, "%s", r64(op->v));
            break;
        case N_NUM:
            snprintf(buf, sz, "%d", op->ival);
            break;
        default:
            snprintf(buf, sz, "%.127s", op->v);
            break;
    }
}

static void tolbl(const char *in, char *out, int sz) {
    int i = 0, j = 0;
    while(in[i] && j < sz - 1) {
        char c = in[i++];
        out[j++] = (isalnum((unsigned char)c) || c == '_') ? c : '_';
    }
    out[j] = 0;
}

static int lbl_ctr = 0;

static void gen_print(OB *ob, const char *lbl) {
    char l[MAX_ID];
    tolbl(lbl, l, sizeof(l));
    ob_l(ob, "    mov rax, 1");
    ob_l(ob, "    mov rdi, 1");
    ob_l(ob, "    mov rsi, %s", l);
    ob_l(ob, "    mov rdx, %s_len", l);
    ob_l(ob, "    syscall");
}

static void gen_node(OB *ob, AN *n);

static void gen_condition(OB *ob, AN *cond, const char *true_lbl, const char *false_lbl) {
    if(!cond || cond->nch < 2) return;
    char a[128] = {0}, b[128] = {0};
    opstr(cond->ch[0], a, 128);
    opstr(cond->ch[1], b, 128);
    ob_l(ob, "    mov rax, %s", a);
    ob_l(ob, "    cmp rax, %s", b);
    if(strcmp(cond->v, ">") == 0 || strcmp(cond->v, "≥") == 0) {
        ob_l(ob, "    jg %s", true_lbl);
    } else if(strcmp(cond->v, ">=") == 0) {
        ob_l(ob, "    jge %s", true_lbl);
    } else if(strcmp(cond->v, "<") == 0) {
        ob_l(ob, "    jl %s", true_lbl);
    } else if(strcmp(cond->v, "<=") == 0) {
        ob_l(ob, "    jle %s", true_lbl);
    } else if(strcmp(cond->v, "==") == 0) {
        ob_l(ob, "    je %s", true_lbl);
    } else if(strcmp(cond->v, "!=") == 0) {
        ob_l(ob, "    jne %s", true_lbl);
    }
    ob_l(ob, "    jmp %s", false_lbl);
}

static void gen_node(OB *ob, AN *n) {
    if(!n) return;
    char a[128] = {0}, b[128] = {0}, lbl[MAX_ID] = {0};
    int i;
    switch(n->type) {
        case N_IMP:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    mov %s, %s", a, b);
            break;
        case N_ADD:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    add %s, %s", a, b);
            break;
        case N_SUB:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    sub %s, %s", a, b);
            break;
        case N_MUL:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    imul %s, %s", a, b);
            break;
        case N_DIV:
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    xor rdx, rdx");
            ob_l(ob, "    idiv %s", b);
            break;
        case N_CLS:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    cmp %s, %s", a, b);
            break;
        case N_JNS:
            tolbl(n->v, lbl, sizeof(lbl));
            ob_l(ob, "    jns %s", lbl);
            break;
        case N_LABEL:
            tolbl(n->v, lbl, sizeof(lbl));
            ob_l(ob, "%s:", lbl);
            break;
        case N_PRINT:
            if(n->v[0]) gen_print(ob, n->v);
            break;
        case N_VAR_DECL:
            var_add(n->v, 0);
            break;
        case N_INPUT:
            ob_l(ob, "    mov rax, 0");
            ob_l(ob, "    mov rdi, 0");
            ob_l(ob, "    mov rsi, _input_buf");
            ob_l(ob, "    mov rdx, 256");
            ob_l(ob, "    syscall");
            break;
        case N_IF: {
            char true_lbl[64], false_lbl[64], end_lbl[64];
            snprintf(true_lbl, sizeof(true_lbl), "_if_true_%d", lbl_ctr);
            snprintf(false_lbl, sizeof(false_lbl), "_if_false_%d", lbl_ctr);
            snprintf(end_lbl, sizeof(end_lbl), "_if_end_%d", lbl_ctr);
            lbl_ctr++;
            if(n->nch >= 1) {
                gen_condition(ob, n->ch[0], true_lbl, false_lbl);
            }
            ob_l(ob, "%s:", true_lbl);
            if(n->nch >= 2) {
                for(i = 0; i < n->ch[1]->nch; i++) {
                    gen_node(ob, n->ch[1]->ch[i]);
                }
            }
            if(n->nch >= 3) {
                ob_l(ob, "    jmp %s", end_lbl);
                ob_l(ob, "%s:", false_lbl);
                for(i = 0; i < n->ch[2]->nch; i++) {
                    gen_node(ob, n->ch[2]->ch[i]);
                }
                ob_l(ob, "%s:", end_lbl);
            } else {
                ob_l(ob, "%s:", false_lbl);
            }
            break;
        }
        case N_FOR:
            for(i = 0; i < n->nch; i++) {
                gen_node(ob, n->ch[i]);
            }
            break;
        case N_MATHS:
            for(i = 0; i < n->nch; i++)
                gen_node(ob, n->ch[i]);
            break;
        case N_REG:
            for(i = 0; i < n->nch; i++)
                gen_node(ob, n->ch[i]);
            break;
        case N_EXEC:
            for(i = 0; i < n->nch; i++)
                gen_node(ob, n->ch[i]);
            break;
        case N_BLOCK:
            for(i = 0; i < n->nch; i++)
                gen_node(ob, n->ch[i]);
            break;
        case N_ASM:
            ob_l(ob, "%s", n->v);
            break;
        default:
            break;
    }
}

static void gen_data(OB *ob, AN *root) {
    int i;
    ob_l(ob, "section .data");
    for(i = 0; i < root->nch; i++) {
        AN *n = root->ch[i];
        if(n->type == N_PSET) {
            char lbl[MAX_ID];
            tolbl(n->v, lbl, sizeof(lbl));
            if(!lbl[0]) snprintf(lbl, sizeof(lbl), "str_%d", i);
            ob_l(ob, "    %s db '%s',0", lbl, n->v);
            ob_l(ob, "    %s_len equ $ - %s", lbl, lbl);
        }
    }
    ob_l(ob, "    _newline db 10");
    ob_l(ob, "");
}

static void gen_bss(OB *ob) {
    ob_l(ob, "section .bss");
    ob_l(ob, "    _input_buf resb 256");
    ob_l(ob, "    _output_buf resb 256");
    ob_l(ob, "    _var_space resb %d", g_var_count * 8);
    ob_l(ob, "");
}

static void codegen(OB *ob, AN *root) {
    int i;
    lbl_ctr = 0;
    ob_l(ob, "; NHP Compiler v%s", NHP_VERSION);
    ob_l(ob, "global _start");
    ob_l(ob, "");
    gen_data(ob, root);
    gen_bss(ob);
    ob_l(ob, "section .text");
    ob_l(ob, "");
    ob_l(ob, "_start:");
    for(i = 0; i < root->nch; i++) {
        AN *n = root->ch[i];
        if(n->type != N_PSET) {
            gen_node(ob, n);
        }
    }
    ob_l(ob, "    mov rax, 60");
    ob_l(ob, "    xor rdi, rdi");
    ob_l(ob, "    syscall");
}

static void do_run(const char *asm_code) {
#if defined(_WIN32)
    fprintf(stderr, "[NHP] Run mode works on Linux/Termux only\n");
    (void)asm_code;
#else
    const char *af = "/tmp/nhp.asm";
    const char *of = "/tmp/nhp.o";
    const char *bf = "/tmp/nhp_out";
    FILE *f = fopen(af, "w");
    if(!f) {
        fprintf(stderr, "[NHP] Cannot write %s\n", af);
        return;
    }
    fputs(asm_code, f);
    fclose(f);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "nasm -f elf64 %s -o %s 2>&1", af, of);
    int r = system(cmd);
    if(r != 0) {
        fprintf(stderr, "[NHP] nasm failed. Install: pkg install nasm\n");
        return;
    }
    snprintf(cmd, sizeof(cmd), "ld %s -o %s 2>&1", of, bf);
    r = system(cmd);
    if(r != 0) {
        fprintf(stderr, "[NHP] ld failed. Install: pkg install binutils\n");
        return;
    }
    snprintf(cmd, sizeof(cmd), "chmod +x %s", bf);
    system(cmd);
    printf("--- Running ---\n");
    system(bf);
    printf("--- Done ---\n");
#endif
}

static char *readfile(const char *path) {
    FILE *f = fopen(path, "rb");
    if(!f) {
        fprintf(stderr, "[NHP] Cannot open: %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = (char*)malloc((size_t)sz + 1);
    if(!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = 0;
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    printf("NHP Compiler v%s\n", NHP_VERSION);
    var_init();
    if(argc < 2) {
        printf("Usage: nhpc file.nhp [-run]\n");
        var_free();
        return 1;
    }
    const char *sf = argv[1];
    int do_run_f = 0;
    if(argc > 2 && strcmp(argv[2], "-run") == 0) {
        do_run_f = 1;
    }
    char *src = readfile(sf);
    if(!src) {
        var_free();
        return 1;
    }
    LX lx;
    lx_init(&lx, src);
    lx_run(&lx);
    err_print_all();
    AN *ast = parse_prog(&lx);
    OB ob;
    ob_init(&ob);
    codegen(&ob, ast);
    FILE *out = fopen("out.asm", "w");
    if(!out) {
        fprintf(stderr, "[NHP] Cannot write out.asm\n");
        ob_free(&ob);
        afree(ast);
        lx_free(&lx);
        free(src);
        var_free();
        return 1;
    }
    fputs(ob.buf, out);
    fclose(out);
    printf("OK: %s -> out.asm (%d bytes)\n", sf, ob.len);
    if(do_run_f) {
        do_run(ob.buf);
    } else {
        printf("Run: nasm -f elf64 out.asm -o out.o && ld out.o -o out && ./out\n");
    }
    ob_free(&ob);
    afree(ast);
    lx_free(&lx);
    free(src);
    var_free();
    return 0;
}
