//Irs Made by NHPDevs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define NHP_VERSION "3.0-ULTIMATE"
#define MAX_TOK      32768
#define MAX_STR      2048
#define MAX_ID       512
#define MAX_CH       128
#define MAX_VARS     9000000  // 9M+ variables support
#define MAX_ERRORS   1024


typedef enum { E_INFO, E_WARN, E_ERROR, E_FATAL, E_SYNTAX } ELevel;

typedef struct {
    int line, col;
    char msg[1024];
    char context[256];
    ELevel level;
    char suggestion[512];
} NHPError;

static NHPError g_errors[MAX_ERRORS];
static int g_nerrors = 0;
static int g_strict = 0;
static int g_show_context = 1;

static void err_push(ELevel lv, int line, int col, const char *fmt, ...) {
    if (g_nerrors >= MAX_ERRORS) return;
    
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_errors[g_nerrors].msg, 1023, fmt, ap);
    va_end(ap);
    
    g_errors[g_nerrors].line = line;
    g_errors[g_nerrors].col = col;
    g_errors[g_nerrors].level = lv;
    g_errors[g_nerrors].context[0] = 0;
    g_errors[g_nerrors].suggestion[0] = 0;
    g_nerrors++;
}

static void err_suggest(const char *suggestion) {
    if (g_nerrors > 0) {
        strncpy(g_errors[g_nerrors-1].suggestion, suggestion, 511);
    }
}

static void err_print_all(void) {
    const char *colors[] = {
        "\033[1;36m", // INFO - Cyan
        "\033[1;33m", // WARN - Yellow
        "\033[1;31m", // ERROR - Red
        "\033[1;35m", // FATAL - Magenta
        "\033[1;91m"  // SYNTAX - Bright Red
    };
    const char *levels[] = {"INFO", "WARNING", "ERROR", "FATAL", "SYNTAX ERROR"};
    
    int i;
    for (i = 0; i < g_nerrors; i++) {
        NHPError *e = &g_errors[i];
        
        fprintf(stderr, "%s╔═══════════════════════════════════════════════════════════\n", 
                colors[e->level]);
        fprintf(stderr, "║ [NHP %s] Line %d, Column %d\n", 
                levels[e->level], e->line, e->col);
        fprintf(stderr, "╠───────────────────────────────────────────────────────────\n");
        fprintf(stderr, "║ %s\n", e->msg);
        
        if (e->context[0]) {
            fprintf(stderr, "║ \n");
            fprintf(stderr, "║ Context: %s\n", e->context);
        }
        
        if (e->suggestion[0]) {
            fprintf(stderr, "║ \n");
            fprintf(stderr, "║ 💡 Suggestion: %s\n", e->suggestion);
        }
        
        fprintf(stderr, "╚═══════════════════════════════════════════════════════════\033[0m\n\n");
    }
    
    if (g_nerrors > 0) {
        int errs = 0, warns = 0, info = 0;
        for (i = 0; i < g_nerrors; i++) {
            if (g_errors[i].level >= E_ERROR) errs++;
            else if (g_errors[i].level == E_WARN) warns++;
            else info++;
        }
        
        fprintf(stderr, "\033[1;97m╔═══════════════════════════════════════════════════════════╗\n");
        fprintf(stderr, "║ compilation                                     ║\n");
        fprintf(stderr, "╠═══════════════════════════════════════════════════════════╣\n");
        fprintf(stderr, "║ \033[1;31mErrors: %-3d\033[1;97m  \033[1;33mWarnings: %-3d\033[1;97m  \033[1;36mInfo: %-3d\033[1;97m                ║\n", 
                errs, warns, info);
        fprintf(stderr, "╚═══════════════════════════════════════════════════════════╝\033[0m\n");
    }
    
    g_nerrors = 0;
}


typedef enum {
    // Core NHP
    TK_PSET, TK_BYTES, TK_LEN, TK_BEGIN, TK_DOLLAR,
    TK_MATHS, TK_REG, TK_REMP, TK_EXECUTE,
    
    // Storage System
    TK_STORAGE, TK_LOAD, TK_LLTD, TK_PARSE, TK_MT, TK_IS, TK_IMR,
    
    // Instructions
    TK_IMP, TK_ADD, TK_SUB, TK_MUL, TK_DIV,
    TK_CLS, TK_JNS, TK_PRINT, TK_ALIAS,
    
    // Registers
    TK_SCR, TK_SCT, TK_SCX, TK_SCZ, TK_SCN,
    
    // UI Elements
    TK_BTN, TK_TEXTBOX, TK_OUTPUT_KW,
    TK_MOUSE_CLICK, TK_CUSTOMER_NAME,
    
    // NEW: Variables & Control Flow
    TK_VAR, TK_MYVAR, TK_IF, TK_ELSE, TK_FOR, TK_WHILE,
    TK_INPUT, TK_INPUTS, TK_RAT, TK_MSG, TK_REN, TK_STC,
    
    // NEW: Operators
    TK_GT, TK_LT, TK_GTE, TK_LTE, TK_NEQ, TK_EQEQ,
    TK_AND, TK_OR, TK_NOT,
    
    // NEW: Server & Network
    TK_SERVER, TK_HTTP, TK_HTTPS, TK_PHP,
    
    // Operators & Literals
    TK_IDENT, TK_NUMBER, TK_STRING, TK_CHAR, TK_LABEL,
    TK_LBR, TK_RBR, TK_LP, TK_RP, TK_LB, TK_RB,
    TK_COMMA, TK_EQ, TK_MINUS, TK_STAR, TK_PLUS,
    TK_COLON, TK_SEMI, TK_HASH, TK_PERCENT,
    TK_AMPERSAND, TK_PIPE, TK_CARET,
    
    TK_TOK, TK_ASM_BLOCK,
    TK_EOF, TK_UNKNOWN
} TK;

typedef struct {
    TK type;
    char val[MAX_STR];
    int line, col;
    int ival;        // Integer value for numbers
    double fval;     // Float value
} Token;

char *w; TK t; } KW;

static KW KWS[] = {
    // Original NHP
    {"Pset", TK_PSET}, {"Maths", TK_MATHS}, {"Reg", TK_REG}, {"Remp", TK_REMP},
    {"Execute_system", TK_EXECUTE},
    {"IMP", TK_IMP}, {"Add", TK_ADD}, {"Sub", TK_SUB}, {"Mul", TK_MUL}, {"Div", TK_DIV},
    {"cls", TK_CLS}, {"jns", TK_JNS}, {"Print", TK_PRINT}, {"print", TK_PRINT},
    {"Alias", TK_ALIAS},
    
    // Registers
    {"SCR", TK_SCR}, {"SCT", TK_SCT}, {"SCX", TK_SCX}, {"SCZ", TK_SCZ}, {"SCN", TK_SCN},
    
    // UI
    {"Textbox", TK_TEXTBOX}, {"Output", TK_OUTPUT_KW},
    {"Mouse2Click", TK_MOUSE_CLICK}, {"CustomerName", TK_CUSTOMER_NAME},
    
    // Storage
    {"LP", TK_LOAD}, {"LLtd", TK_LLTD}, {"Parse", TK_PARSE},
    {"IS", TK_IS}, {"IMR", TK_IMR}, {"MT", TK_MT}, {"mt", TK_MT},
    {"Tok", TK_TOK},
    
    // NEW: Variables & Control
    {"var", TK_VAR}, {"My", TK_MYVAR}, {"my", TK_MYVAR},
    {"if", TK_IF}, {"If", TK_IF}, {"IF", TK_IF},
    {"else", TK_ELSE}, {"Else", TK_ELSE}, {"ELSE", TK_ELSE},
    {"For", TK_FOR}, {"for", TK_FOR},
    {"while", TK_WHILE}, {"While", TK_WHILE},
    
    // NEW: Input/Output
    {"InpuS", TK_INPUTS}, {"Rat", TK_RAT}, {"msg", TK_MSG},
    {"ren", TK_REN}, {"stc", TK_STC},
    
    // NEW: Server
    {"Server", TK_SERVER}, {"HTTP", TK_HTTP}, {"HTTPS", TK_HTTPS},
    {"PHP", TK_PHP},
    
    {NULL, TK_UNKNOWN}
};


typedef struct {
    char name[MAX_ID];
    int type;  // 0=int, 1=float, 2=string
    union {
        int ival;
        double fval;
        char *sval;
    } value;
    int scope;
    int used;
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
    for (i = 0; i < g_var_count; i++) {
        if (g_vars[i].type == 2 && g_vars[i].value.sval) {
            free(g_vars[i].value.sval);
        }
    }
    free(g_vars);
}

static int var_add(const char *name, int type) {
    if (g_var_count >= g_var_capacity) {
        g_var_capacity *= 2;
        if (g_var_capacity > MAX_VARS) g_var_capacity = MAX_VARS;
        g_vars = (Variable*)realloc(g_vars, g_var_capacity * sizeof(Variable));
    }
    
    strncpy(g_vars[g_var_count].name, name, MAX_ID-1);
    g_vars[g_var_count].type = type;
    g_vars[g_var_count].used = 0;
    g_vars[g_var_count].scope = 0;
    
    return g_var_count++;
}

static int var_find(const char *name) {
    int i;
    for (i = g_var_count - 1; i >= 0; i--) {
        if (strcmp(g_vars[i].name, name) == 0) {
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
    b->cap = 1024 * 1024;  // 1MB initial
    b->buf = (char*)malloc(b->cap);
    b->buf[0] = 0;
    b->len = 0;
}

static void ob_free(OB *b) {
    free(b->buf);
    b->buf = NULL;
}

static void ob_grow(OB *b, int n) {
    while (b->len + n + 4 >= b->cap) {
        b->cap *= 2;
        b->buf = (char*)realloc(b->buf, b->cap);
    }
}

static void ob_w(OB *b, const char *fmt, ...) {
    va_list ap;
    ob_grow(b, 4096);
    va_start(ap, fmt);
    int n = vsnprintf(b->buf + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    if (n > 0 && b->len + n < b->cap) b->len += n;
}

static void ob_l(OB *b, const char *fmt, ...) {
    va_list ap;
    ob_grow(b, 4096);
    va_start(ap, fmt);
    int n = vsnprintf(b->buf + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    if (n > 0 && b->len + n < b->cap) {
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
    char *src_lines[10000];
    int num_lines;
} LX;

static void lx_init(LX *l, const char *s) {
    l->src = s;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
    l->cap = 2048;
    l->cnt = 0;
    l->t = (Token*)malloc(l->cap * sizeof(Token));
    l->num_lines = 0;
}

static void lx_free(LX *l) {
    free(l->t);
}

static char lp(LX *l) {
    return l->src[l->pos];
}

static char lp2(LX *l) {
    return l->src[l->pos] ? l->src[l->pos+1] : 0;
}

static char la(LX *l) {
    char c = l->src[l->pos++];
    if (c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

static void lpush(LX *l, TK t, const char *v, int ln, int col) {
    if (l->cnt >= l->cap) {
        l->cap *= 2;
        l->t = (Token*)realloc(l->t, l->cap * sizeof(Token));
    }
    
    l->t[l->cnt].type = t;
    l->t[l->cnt].line = ln;
    l->t[l->cnt].col = col;
    strncpy(l->t[l->cnt].val, v ? v : "", MAX_STR-1);
    l->t[l->cnt].ival = 0;
    l->t[l->cnt].fval = 0.0;
    
    if (t == TK_NUMBER && v) {
        l->t[l->cnt].ival = (int)strtol(v, NULL, 0);
        l->t[l->cnt].fval = strtod(v, NULL);
    }
    
    l->cnt++;
}

static void skip_line_comment(LX *l) {
    while (lp(l) && lp(l) != '\n') la(l);
}

static void skip_block_comment(LX *l) {
    la(l); la(l);  // skip /*
    while (lp(l)) {
        if (lp(l) == '*' && lp2(l) == '/') {
            la(l); la(l);
            break;
        }
        la(l);
    }
}

static void lx_run(LX *l) {
    while (lp(l)) {
        // Skip whitespace
        while (lp(l) && (lp(l) == ' ' || lp(l) == '\t' || lp(l) == '\r' || lp(l) == '\n'))
            la(l);
        
        if (!lp(l)) break;
        
        int ln = l->line, co = l->col;
        char c = lp(l);
        const char *s = &l->src[l->pos];
        
        // Comments
        if (c == ';' || (c == '/' && lp2(l) == '/')) {
            skip_line_comment(l);
            continue;
        }
        
        if (c == '/' && lp2(l) == '*') {
            skip_block_comment(l);
            continue;
        }
        
        // ERROR: Wrong comment syntax
        if (c == '#') {
            err_push(E_ERROR, ln, co, "'#' is not a valid comment in NHP");
            err_suggest("Use ';' or '//' for comments");
            skip_line_comment(l);
            continue;
        }
        
        if (c == '-' && lp2(l) == '-') {
            err_push(E_ERROR, ln, co, "'--' is not a valid comment in NHP");
            err_suggest("Use ';' or '//' for comments");
            skip_line_comment(l);
            continue;
        }
        
        // Multi-character operators
        if (c == '>' && lp2(l) == '=') {
            la(l); la(l);
            lpush(l, TK_GTE, ">=", ln, co);
            continue;
        }
        
        if (c == '<' && lp2(l) == '=') {
            la(l); la(l);
            lpush(l, TK_LTE, "<=", ln, co);
            continue;
        }
        
        if (c == '!' && lp2(l) == '=') {
            la(l); la(l);
            lpush(l, TK_NEQ, "!=", ln, co);
            continue;
        }
        
        if (c == '=' && lp2(l) == '=') {
            la(l); la(l);
            lpush(l, TK_EQEQ, "==", ln, co);
            continue;
        }
        
        if (c == '&' && lp2(l) == '&') {
            la(l); la(l);
            lpush(l, TK_AND, "&&", ln, co);
            continue;
        }
        
        if (c == '|' && lp2(l) == '|') {
            la(l); la(l);
            lpush(l, TK_OR, "||", ln, co);
            continue;
        }
        
        // Special NHP sequences
        if (strncmp(s, "I*Storage", 9) == 0) {
            int i; for (i = 0; i < 9; i++) la(l);
            lpush(l, TK_STORAGE, "I*Storage", ln, co);
            continue;
        }
        
        if (strncmp(s, "InpuS", 5) == 0) {
            int i; for (i = 0; i < 5; i++) la(l);
            lpush(l, TK_INPUTS, "InpuS", ln, co);
            continue;
        }
        
        if (strncmp(s, "Rat//", 5) == 0) {
            int i; for (i = 0; i < 5; i++) la(l);
            lpush(l, TK_RAT, "Rat//", ln, co);
            continue;
        }
        
        if (strncmp(s, "//*msg", 6) == 0) {
            int i; for (i = 0; i < 6; i++) la(l);
            lpush(l, TK_MSG, "//*msg", ln, co);
            continue;
        }
        
        // String literals
        if (c == '"') {
            la(l);
            char buf[MAX_STR] = {0};
            int bi = 0;
            
            while (lp(l) && lp(l) != '"') {
                if (lp(l) == '\\') {
                    la(l);
                    char e = la(l);
                    switch (e) {
                        case 'n': buf[bi++] = '\n'; break;
                        case 't': buf[bi++] = '\t'; break;
                        case 'r': buf[bi++] = '\r'; break;
                        case '\\': buf[bi++] = '\\'; break;
                        case '"': buf[bi++] = '"'; break;
                        default: if (bi < MAX_STR-1) buf[bi++] = e;
                    }
                } else {
                    if (bi < MAX_STR-1) buf[bi++] = la(l);
                    else la(l);
                }
            }
            
            if (lp(l) == '"') la(l);
            else err_push(E_ERROR, ln, co, "Unterminated string literal");
            
            lpush(l, TK_STRING, buf, ln, co);
            continue;
        }
        
        // Character literals
        if (c == '\'') {
            la(l);
            char buf[4] = {0};
            
            if (lp(l) == '\\') {
                la(l);
                char e = la(l);
                switch (e) {
                    case 'n': buf[0] = '\n'; break;
                    case 't': buf[0] = '\t'; break;
                    case 'r': buf[0] = '\r'; break;
                    case '\\': buf[0] = '\\'; break;
                    case '\'': buf[0] = '\''; break;
                    default: buf[0] = e;
                }
            } else {
                buf[0] = la(l);
            }
            
            if (lp(l) == '\'') la(l);
            else err_push(E_ERROR, ln, co, "Unterminated character literal");
            
            lpush(l, TK_CHAR, buf, ln, co);
            continue;
        }
        
        // Numbers (including hex)
        if (isdigit((unsigned char)c) || (c == '0' && (lp2(l) == 'x' || lp2(l) == 'X'))) {
            char buf[128] = {0};
            int bi = 0;
            
            if (c == '0' && (lp2(l) == 'x' || lp2(l) == 'X')) {
                buf[bi++] = la(l);
                buf[bi++] = la(l);
                while (isxdigit((unsigned char)lp(l)) && bi < 126)
                    buf[bi++] = la(l);
            } else {
                while ((isdigit((unsigned char)lp(l)) || lp(l) == '.') && bi < 126)
                    buf[bi++] = la(l);
            }
            
            lpush(l, TK_NUMBER, buf, ln, co);
            continue;
        }
        
        // Identifiers and keywords
        if (isalpha((unsigned char)c) || c == '_') {
            char buf[MAX_ID] = {0};
            int bi = 0;
            
            while (lp(l) && (isalnum((unsigned char)lp(l)) || lp(l) == '_' || lp(l) == '.') && bi < MAX_ID-1)
                buf[bi++] = la(l);
            
            // Check for label
            if (lp(l) == ':') {
                la(l);
                lpush(l, TK_LABEL, buf, ln, co);
                continue;
            }
            
            // Keyword lookup
            int k, found = 0;
            for (k = 0; KWS[k].w; k++) {
                if (strcmp(buf, KWS[k].w) == 0) {
                    lpush(l, KWS[k].t, buf, ln, co);
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                lpush(l, TK_IDENT, buf, ln, co);
            }
            
            continue;
        }
        
        // Single character tokens
        la(l);
        switch (c) {
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
            case '|': lpush(l, TK_PIPE, "|", ln, co); break;
            case '^': lpush(l, TK_CARET, "^", ln, co); break;
            case ';': lpush(l, TK_SEMI, ";", ln, co); break;
            case ':': lpush(l, TK_COLON, ":", ln, co); break;
            case '>': lpush(l, TK_GT, ">", ln, co); break;
            case '<': lpush(l, TK_LT, "<", ln, co); break;
            case '!': lpush(l, TK_NOT, "!", ln, co); break;
            default:
                if (!isspace((unsigned char)c)) {
                    char tmp[8] = {c, 0};
                    err_push(E_WARN, ln, co, "Unknown character '%s' ignored", tmp);
                }
                break;
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
    
    // NEW
    N_VAR_DECL, N_VAR_ASSIGN, N_IF, N_ELSE, N_FOR, N_WHILE,
    N_INPUT, N_CONDITION, N_BLOCK,
    N_SERVER, N_HTTP,
    
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
    if (p && c && p->nch < MAX_CH)
        p->ch[p->nch++] = c;
}

static void afree(AN *n) {
    if (!n) return;
    int i;
    for (i = 0; i < n->nch; i++)
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
    if (pa(p, t)) pe(p);
    else {
        err_push(E_SYNTAX, pc(p)->line, pc(p)->col, 
                "Expected token type %d, got %d", t, pc(p)->type);
    }
}

static AN *parse_expr(PR *p);
static AN *parse_stmt(PR *p);

// Parse operand (register, number, string, identifier)
static AN *pop2(PR *p) {
    AN *n = NULL;
    int ln = pc(p)->line;
    
    switch (pc(p)->type) {
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
            if (pa(p, TK_NUMBER)) {
                Token *t = pe(p);
                n->ival = -t->ival;
                n->fval = -t->fval;
                snprintf(n->v, MAX_STR, "%d", n->ival);
            }
            break;
            
        case TK_AMPERSAND:  // &myvar
            pe(p);
            if (pa(p, TK_IDENT)) {
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

// Parse binary operation
static AN *p2op(PR *p, NT nt) {
    int ln = pc(p)->line;
    pe(p);
    
    AN *n = an(nt, ln);
    AN *a = pop2(p);
    if (a) ac(n, a);
    
    pm(p, TK_COMMA);
    
    AN *b = pop2(p);
    if (b) ac(n, b);
    
    return n;
}

// Parse condition
static AN *parse_condition(PR *p) {
    AN *n = an(N_CONDITION, pc(p)->line);
    
    // Left operand
    AN *left = pop2(p);
    if (left) ac(n, left);
    
    // Operator
    TK op = pc(p)->type;
    strcpy(n->v, pc(p)->val);
    
    if (op == TK_GT || op == TK_LT || op == TK_GTE || op == TK_LTE || 
        op == TK_EQEQ || op == TK_NEQ) {
        pe(p);
    } else {
        err_push(E_SYNTAX, pc(p)->line, pc(p)->col, 
                "Expected comparison operator, got '%s'", pc(p)->val);
    }
    
    // Right operand
    AN *right = pop2(p);
    if (right) ac(n, right);
    
    return n;
}

// Parse statement
static AN *parse_stmt(PR *p) {
    int ln = pc(p)->line;
    
    switch (pc(p)->type) {
        // Instructions
        case TK_IMP: return p2op(p, N_IMP);
        case TK_ADD: return p2op(p, N_ADD);
        case TK_SUB: return p2op(p, N_SUB);
        case TK_MUL: return p2op(p, N_MUL);
        case TK_DIV: return p2op(p, N_DIV);
        case TK_CLS: return p2op(p, N_CLS);
        
        case TK_JNS: {
            pe(p);
            AN *n = an(N_JNS, ln);
            if (pa(p, TK_IDENT) || pa(p, TK_LABEL))
                strcpy(n->v, pe(p)->val);
            return n;
        }
        
        // Print statement
        case TK_PRINT: {
            pe(p);
            AN *n = an(N_PRINT, ln);
            pm(p, TK_LP);
            
            if (pa(p, TK_STRING)) {
                strcpy(n->v, pe(p)->val);
            }
            
            // Handle format specifiers
            while (pa(p, TK_COMMA)) {
                pe(p);
                AN *arg = pop2(p);
                if (arg) ac(n, arg);
            }
            
            pm(p, TK_RP);
            return n;
        }
        
        // Label
        case TK_BEGIN: case TK_LABEL: {
            AN *n = an(N_LABEL, ln);
            strcpy(n->v, pe(p)->val);
            return n;
        }
        
        // Variable declaration: My var
        case TK_MYVAR: {
            pe(p);
            AN *n = an(N_VAR_DECL, ln);
            
            if (pa(p, TK_IDENT)) {
                strcpy(n->v, pe(p)->val);
                var_add(n->v, 0);  // Register variable
            }
            
            return n;
        }
        
        // Input: InpuS("%d", &num)
        case TK_INPUTS: {
            pe(p);
            AN *n = an(N_INPUT, ln);
            
            pm(p, TK_LP);
            
            if (pa(p, TK_STRING)) {
                strcpy(n->v, pe(p)->val);  // Format string
            }
            
            pm(p, TK_COMMA);
            
            // Variable reference
            AN *var = pop2(p);
            if (var) ac(n, var);
            
            pm(p, TK_RP);
            
            return n;
        }
        
        // If statement
        case TK_IF: {
            pe(p);
            AN *n = an(N_IF, ln);
            
            // Parse condition
            AN *cond = parse_condition(p);
            if (cond) ac(n, cond);
            
            // Parse body
            pm(p, TK_LB);
            
            AN *block = an(N_BLOCK, ln);
            while (!pa(p, TK_RB) && !pa(p, TK_EOF)) {
                AN *stmt = parse_stmt(p);
                if (stmt) ac(block, stmt);
            }
            ac(n, block);
            
            pm(p, TK_RB);
            
            // Optional else
            if (pa(p, TK_ELSE)) {
                pe(p);
                pm(p, TK_LB);
                
                AN *else_block = an(N_BLOCK, ln);
                while (!pa(p, TK_RB) && !pa(p, TK_EOF)) {
                    AN *stmt = parse_stmt(p);
                    if (stmt) ac(else_block, stmt);
                }
                ac(n, else_block);
                
                pm(p, TK_RB);
            }
            
            return n;
        }
        
        // For loop
        case TK_FOR: {
            pe(p);
            AN *n = an(N_FOR, ln);
            
            pm(p, TK_LP);
            
            // Init
            if (pa(p, TK_IF)) {
                pe(p);
                pm(p, TK_STAR);
            }
            
            // Consume loop structure
            while (!pa(p, TK_RP) && !pa(p, TK_EOF)) {
                pe(p);
            }
            
            pm(p, TK_RP);
            
            // Body
            pm(p, TK_LB);
            
            AN *block = an(N_BLOCK, ln);
            while (!pa(p, TK_RB) && !pa(p, TK_EOF)) {
                AN *stmt = parse_stmt(p);
                if (stmt) ac(block, stmt);
            }
            ac(n, block);
            
            pm(p, TK_RB);
            
            return n;
        }
        
        default:
            pe(p);
            return NULL;
    }
}

// Parse program
static AN *parse_prog(LX *lx) {
    PR p;
    p.t = lx->t;
    p.cnt = lx->cnt;
    p.pos = 0;
    
    AN *root = an(N_PROG, 0);
    strcpy(root->v, "program");
    
    while (!pa(&p, TK_EOF)) {
        AN *n = NULL;
        int ln = pc(&p)->line;
        
        switch (pc(&p)->type) {
            case TK_PSET: {
                pe(&p);
                n = an(N_PSET, ln);
                
                if (pa(&p, TK_STRING))
                    strcpy(n->v, pe(&p)->val);
                else if (pa(&p, TK_IDENT))
                    strcpy(n->v, pe(&p)->val);
                
                break;
            }
            
            case TK_MATHS: {
                pe(&p);
                n = an(N_MATHS, ln);
                
                pm(&p, TK_LB);
                
                while (!pa(&p, TK_RB) && !pa(&p, TK_EOF)) {
                    AN *stmt = parse_stmt(&p);
                    if (stmt) ac(n, stmt);
                }
                
                pm(&p, TK_RB);
                break;
            }
            
            case TK_REG: {
                pe(&p);
                n = an(N_REG, ln);
                
                pm(&p, TK_LB);
                
                while (!pa(&p, TK_RB) && !pa(&p, TK_EOF)) {
                    AN *stmt = parse_stmt(&p);
                    if (stmt) ac(n, stmt);
                }
                
                pm(&p, TK_RB);
                break;
            }
            
            case TK_EXECUTE: {
                pe(&p);
                n = an(N_EXEC, ln);
                
                pm(&p, TK_LB);
                
                while (!pa(&p, TK_RB) && !pa(&p, TK_EOF)) {
                    AN *stmt = parse_stmt(&p);
                    if (stmt) ac(n, stmt);
                }
                
                pm(&p, TK_RB);
                break;
            }
            
            default:
                n = parse_stmt(&p);
                break;
        }
        
        if (n) ac(root, n);
    }
    
    return root;
}


static const char *r64(const char *r) {
    if (!strcmp(r, "SCR")) return "rax";
    if (!strcmp(r, "SCT")) return "rbx";
    if (!strcmp(r, "SCX")) return "rcx";
    if (!strcmp(r, "SCZ")) return "rdx";
    if (!strcmp(r, "SCN")) return "r8";
    return r;
}

static void opstr(AN *op, char *buf, int sz) {
    if (!op) {
        buf[0] = 0;
        return;
    }
    
    switch (op->type) {
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
    while (in[i] && j < sz-1) {
        char c = in[i++];
        out[j++] = (isalnum((unsigned char)c) || c == '_') ? c : '_';
    }
    out[j] = 0;
}

static int lbl_ctr = 0;
static int var_offset = 0;

static void gen_print(OB *ob, const char *lbl) {
    char l[MAX_ID];
    tolbl(lbl, l, sizeof(l));
    
    ob_l(ob, "    ; Print '%s'", lbl);
    ob_l(ob, "    mov  rax, 1");
    ob_l(ob, "    mov  rdi, 1");
    ob_l(ob, "    mov  rsi, %s", l);
    ob_l(ob, "    mov  rdx, %s_len", l);
    ob_l(ob, "    syscall");
}

static void gen_node(OB *ob, AN *n);

static void gen_condition(OB *ob, AN *cond, const char *true_lbl, const char *false_lbl) {
    if (!cond || cond->nch < 2) return;
    
    char a[128] = {0}, b[128] = {0};
    opstr(cond->ch[0], a, 128);
    opstr(cond->ch[1], b, 128);
    
    ob_l(ob, "    ; Condition: %s %s %s", a, cond->v, b);
    ob_l(ob, "    mov  rax, %s", a);
    ob_l(ob, "    cmp  rax, %s", b);
    
    if (strcmp(cond->v, ">") == 0 || strcmp(cond->v, "≥") == 0) {
        ob_l(ob, "    jg   %s", true_lbl);
    } else if (strcmp(cond->v, ">=") == 0) {
        ob_l(ob, "    jge  %s", true_lbl);
    } else if (strcmp(cond->v, "<") == 0) {
        ob_l(ob, "    jl   %s", true_lbl);
    } else if (strcmp(cond->v, "<=") == 0) {
        ob_l(ob, "    jle  %s", true_lbl);
    } else if (strcmp(cond->v, "==") == 0) {
        ob_l(ob, "    je   %s", true_lbl);
    } else if (strcmp(cond->v, "!=") == 0) {
        ob_l(ob, "    jne  %s", true_lbl);
    }
    
    ob_l(ob, "    jmp  %s", false_lbl);
}

static void gen_node(OB *ob, AN *n) {
    if (!n) return;
    
    char a[128] = {0}, b[128] = {0}, lbl[MAX_ID] = {0};
    int i;
    
    switch (n->type) {
        case N_IMP:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    mov  %s, %s", a, b);
            break;
            
        case N_ADD:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    add  %s, %s", a, b);
            break;
            
        case N_SUB:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    sub  %s, %s", a, b);
            break;
            
        case N_MUL:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    imul %s, %s", a, b);
            break;
            
        case N_DIV:
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    xor  rdx, rdx");
            ob_l(ob, "    idiv %s", b);
            break;
            
        case N_CLS:
            opstr(n->nch >= 1 ? n->ch[0] : NULL, a, 128);
            opstr(n->nch >= 2 ? n->ch[1] : NULL, b, 128);
            ob_l(ob, "    cmp  %s, %s", a, b);
            break;
            
        case N_JNS:
            tolbl(n->v, lbl, sizeof(lbl));
            ob_l(ob, "    jns  %s", lbl);
            break;
            
        case N_LABEL:
            tolbl(n->v, lbl, sizeof(lbl));
            ob_l(ob, "%s:", lbl);
            break;
            
        case N_PRINT:
            if (n->v[0]) gen_print(ob, n->v);
            break;
            
        case N_VAR_DECL:
            ob_l(ob, "    ; Variable: %s", n->v);
            var_add(n->v, 0);
            break;
            
        case N_INPUT:
            ob_l(ob, "    ; Input: scanf(\"%s\")", n->v);
            ob_l(ob, "    mov  rax, 0");
            ob_l(ob, "    mov  rdi, 0");
            ob_l(ob, "    mov  rsi, _input_buf");
            ob_l(ob, "    mov  rdx, 256");
            ob_l(ob, "    syscall");
            break;
            
        case N_IF: {
            char true_lbl[64], false_lbl[64], end_lbl[64];
            snprintf(true_lbl, sizeof(true_lbl), "_if_true_%d", lbl_ctr);
            snprintf(false_lbl, sizeof(false_lbl), "_if_false_%d", lbl_ctr);
            snprintf(end_lbl, sizeof(end_lbl), "_if_end_%d", lbl_ctr);
            lbl_ctr++;
            
            ob_l(ob, "    ; If statement");
            
            if (n->nch >= 1) {
                gen_condition(ob, n->ch[0], true_lbl, false_lbl);
            }
            
            ob_l(ob, "%s:", true_lbl);
            if (n->nch >= 2) {
                for (i = 0; i < n->ch[1]->nch; i++) {
                    gen_node(ob, n->ch[1]->ch[i]);
                }
            }
            
            if (n->nch >= 3) {
                ob_l(ob, "    jmp  %s", end_lbl);
                ob_l(ob, "%s:", false_lbl);
                for (i = 0; i < n->ch[2]->nch; i++) {
                    gen_node(ob, n->ch[2]->ch[i]);
                }
                ob_l(ob, "%s:", end_lbl);
            } else {
                ob_l(ob, "%s:", false_lbl);
            }
            
            break;
        }
        
        case N_FOR:
            ob_l(ob, "    ; For loop");
            for (i = 0; i < n->nch; i++) {
                gen_node(ob, n->ch[i]);
            }
            break;
            
        case N_MATHS:
            ob_l(ob, "    ; === Maths section ===");
            for (i = 0; i < n->nch; i++)
                gen_node(ob, n->ch[i]);
            ob_l(ob, "    ; === end Maths ===");
            break;
            
        case N_REG:
            ob_l(ob, "    ; === Reg section ===");
            for (i = 0; i < n->nch; i++)
                gen_node(ob, n->ch[i]);
            ob_l(ob, "    ; === end Reg ===");
            break;
            
        case N_EXEC:
            ob_l(ob, "    ; === Execute_system ===");
            for (i = 0; i < n->nch; i++)
                gen_node(ob, n->ch[i]);
            ob_l(ob, "    ; === end Execute_system ===");
            break;
            
        case N_BLOCK:
            for (i = 0; i < n->nch; i++)
                gen_node(ob, n->ch[i]);
            break;
            
        default:
            break;
    }
}

static void gen_data(OB *ob, AN *root) {
    int i;
    
    ob_l(ob, "section .data");
    
    for (i = 0; i < root->nch; i++) {
        AN *n = root->ch[i];
        
        if (n->type == N_PSET) {
            char lbl[MAX_ID];
            tolbl(n->v, lbl, sizeof(lbl));
            if (!lbl[0]) snprintf(lbl, sizeof(lbl), "str_%d", i);
            
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
    
    ob_l(ob, "; ════════════════════════════════════════════════════════════════════");
    ob_l(ob, "; NHP Compiler v%s - ULTIMATE EDITION", NHP_VERSION);
    ob_l(ob, "; The World's Fastest Programming Language");
    ob_l(ob, "; Faster than Black Flash - Binary Level Optimization");
    ob_l(ob, "; ════════════════════════════════════════════════════════════════════");
    ob_l(ob, ";");
    ob_l(ob, "; Target: x86-64 Linux");
    ob_l(ob, "; Optimization: -O3 -march=native -mtune=native");
    ob_l(ob, ";");
    ob_l(ob, "; Registers:");
    ob_l(ob, ";   SCR = RAX   (Accumulator)");
    ob_l(ob, ";   SCT = RBX   (Base)");
    ob_l(ob, ";   SCX = RCX   (Counter)");
    ob_l(ob, ";   SCZ = RDX   (Data)");
    ob_l(ob, ";   SCN = R8    (Numeric)");
    ob_l(ob, ";");
    ob_l(ob, "; Features:");
    ob_l(ob, ";   MY Data Compiler");
    ob_l(ob, ";     ");
    ob_l(ob, ";     ");
    ob_l(ob, ";    it's Made by Jatin the OG Creator");
    ob_l(ob, ";   join Our Discord - https://discord.gg/zTpmMEthp");
    ob_l(ob, "; ════════════════════════════════════════════════════════════════════");
    ob_l(ob, "");
    
    ob_l(ob, "global _start");
    ob_l(ob, "");
    
    gen_data(ob, root);
    gen_bss(ob);
    
    ob_l(ob, "section .text");
    ob_l(ob, "");
    ob_l(ob, "_start:");
    
    for (i = 0; i < root->nch; i++) {
        AN *n = root->ch[i];
        if (n->type != N_PSET) {
            gen_node(ob, n);
        }
    }
    
    ob_l(ob, "");
    ob_l(ob, "    ; ════ Program Exit ════");
    ob_l(ob, "    mov  rax, 60    ; sys_exit");
    ob_l(ob, "    xor  rdi, rdi   ; status 0");
    ob_l(ob, "    syscall");
    ob_l(ob, "");
    ob_l(ob, "; ════════════════════════════════════════════════════════════════════");
    ob_l(ob, "; End of MY Data Code");
    ob_l(ob, "; MY Data Codes v%s", NHP_Version);
    ob_l(ob, "; ════════════════════════════════════════════════════════════════════");
}

static void hex_dump(const char *d, int len) {
    int i;
    
  
    
    for (i = 0; i < len; i += 16) {
        printf("║ %08x  ", i);
        
        int j;
        for (j = 0; j < 16; j++) {
            if (i + j < len)
                printf("%02x ", (unsigned char)d[i+j]);
            else
                printf("   ");
            if (j == 7) printf(" ");
        }
        
        printf(" │ ");
        
        for (j = 0; j < 16 && i + j < len; j++) {
            unsigned char c = (unsigned char)d[i+j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        
        printf(" ║\n");
    }
    
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");
}

static void gen_php(OB *ob, AN *root) {
    ob_l(ob, "<?php");
    ob_l(ob, "/**");
    ob_l(ob, " * MY Data");
    ob_l(ob, " * Generated by NHP Compiler v%s", NHP_VERSION);
    ob_l(ob, " * SS enabled");
    ob_l(ob, " */");
    ob_l(ob, "");
    ob_l(ob, "// NHP Runtime");
    ob_l(ob, "class NHPRuntime {");
    ob_l(ob, "    private $vars = [];");
    ob_l(ob, "    private $registers = ['SCR'=>0,'SCT'=>0,'SCX'=>0,'SCZ'=>0,'SCN'=>0];");
    ob_l(ob, "    ");
    ob_l(ob, "    public function IMP($dst, $src) {");
    ob_l(ob, "        $this->registers[$dst] = $src;");
    ob_l(ob, "    }");
    ob_l(ob, "    ");
    ob_l(ob, "    public function Add($dst, $src) {");
    ob_l(ob, "        $this->registers[$dst] += $src;");
    ob_l(ob, "    }");
    ob_l(ob, "    ");
    ob_l(ob, "    public function Print($msg) {");
    ob_l(ob, "        echo $msg . \"\\n\";");
    ob_l(ob, "    }");
    ob_l(ob, "}");
    ob_l(ob, "");
    ob_l(ob, "$nhp = new NHPRuntime();");
    ob_l(ob, "");
    ob_l(ob, "// Program execution");
    ob_l(ob, "?>");
}

/* ═══════════════════════════════════════════════════════════════════════
 * EXECUTION ENGINE
 * ═══════════════════════════════════════════════════════════════════════ */

static void do_run(const char *asm_code, int verbose) {
#if defined(_WIN32)
    fprintf(stderr, "[NHP] -run is Linux/Termux only\n");
    (void)asm_code;
    (void)verbose;
#else
    const char *af = "/tmp/nhp.asm";
    const char *of = "/tmp/nhp.o";
    const char *bf = "/tmp/nhp_out";
    
    FILE *f = fopen(af, "w");
    if (!f) {
        fprintf(stderr, "[NHP] Cannot write %s\n", af);
        return;
    }
    
    fputs(asm_code, f);
    fclose(f);
    
    if (verbose) printf("[NHP] nasm -f elf64 %s -o %s\n", af, of);
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "nasm -f elf64 %s -o %s 2>&1", af, of);
    int r = system(cmd);
    
    if (r != 0) {
        fprintf(stderr, "\n");
        fprintf(stderr, "╔════════════════════════════════════════════════════════════════════╗\n");
        fprintf(stderr, "║ ERROR: NASM assembly failed                                        ║\n");
        fprintf(stderr, "╠════════════════════════════════════════════════════════════════════╣\n");
        fprintf(stderr, "║ Install NASM:                                                      ║\n");
        fprintf(stderr, "║   Termux:  pkg install nasm                                        ║\n");
        fprintf(stderr, "║   Ubuntu:  sudo apt install nasm                                   ║\n");
        fprintf(stderr, "╚════════════════════════════════════════════════════════════════════╝\n");
        return;
    }
    
    snprintf(cmd, sizeof(cmd), "ld %s -o %s 2>&1", of, bf);
    r = system(cmd);
    
    if (r != 0) {
        fprintf(stderr, "\n");
        fprintf(stderr, "╔════════════════════════════════════════════════════════════════════╗\n");
        fprintf(stderr, "║ ERROR: Linking failed                                              ║\n");
        fprintf(stderr, "╠════════════════════════════════════════════════════════════════════╣\n");
        fprintf(stderr, "║ Install binutils:                                                  ║\n");
        fprintf(stderr, "║   Termux:  pkg install binutils                                    ║\n");
        fprintf(stderr, "║   Ubuntu:  sudo apt install binutils                               ║\n");
        fprintf(stderr, "╚════════════════════════════════════════════════════════════════════╝\n");
        return;
    }
    
    snprintf(cmd, sizeof(cmd), "chmod +x %s", bf);
    system(cmd);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ RUNNING NHP PROGRAM                                                    ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════╣\n");
    
    r = system(bf);
    
    printf("╠════════════════════════════════════════════════════════════════════════╣\n");
    if (WIFEXITED(r)) {
        printf("║ Exit Code: %d                                                           ║\n", 
               WEXITSTATUS(r));
    }
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");
#endif
}


static void repl(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║  MY Data REPL v%s - MY Data                               ║\n", NHP_VERSION);
    printf("║  TSS Help Commands                          ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════╣\n");
    printf("║  Commands:                                                             ║\n");
    printf("║    run      - Compile and execute                                      ║\n");
    printf("║    asm      - Show assembly code                                       ║\n");
    printf("║    hex      - Show hex dump                                            ║\n");
    printf("║    show     - Display source                                           ║\n");
    printf("║    clear    - Clear buffer                                             ║\n");
    printf("║    tokens   - Show token stream                                        ║\n");
    printf("║    help     - Show this help                                           ║\n");
    printf("║    exit     - Quit REPL                                                ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n\n");
    
    var_init();
    
    int srcap = 65536, srclen = 0;
    char *srcbuf = (char*)malloc(srcap);
    srcbuf[0] = 0;
    
    char line[4096];
    int running = 1;
    
    while (running) {
        printf("\033[1;32mnhp>\033[0m ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n[NHP] EOF. Goodbye.\n");
            break;
        }
        
        int ll = (int)strlen(line);
        while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\r'))
            line[--ll] = 0;
        
        if (!strcmp(line, "exit") || !strcmp(line, "quit") || !strcmp(line, ".exit")) {
            printf("[NHP] Goodbye.\n");
            running = 0;
            break;
        }
        
        if (!strcmp(line, "clear") || !strcmp(line, ".clear")) {
            srclen = 0;
            srcbuf[0] = 0;
            printf("[NHP] Buffer cleared.\n");
            continue;
        }
        
        if (!strcmp(line, "show") || !strcmp(line, ".show")) {
            printf("───────────────────────────────────────────────────────────\n");
            printf("%s", srclen ? srcbuf : "(empty)\n");
            printf("───────────────────────────────────────────────────────────\n");
            continue;
        }
        
        if (!strcmp(line, "run") || !strcmp(line, ".run")) {
            if (!srclen) {
                printf("[NHP] Buffer is empty.\n");
                continue;
            }
            
            LX lx;
            lx_init(&lx, srcbuf);
            lx_run(&lx);
            err_print_all();
            
            AN *ast = parse_prog(&lx);
            
            OB ob;
            ob_init(&ob);
            codegen(&ob, ast);
            
            do_run(ob.buf, 0);
            
            ob_free(&ob);
            afree(ast);
            lx_free(&lx);
            continue;
        }
        
        if (!strcmp(line, "asm") || !strcmp(line, ".asm")) {
            if (!srclen) {
                printf("[NHP] Buffer is empty.\n");
                continue;
            }
            
            LX lx;
            lx_init(&lx, srcbuf);
            lx_run(&lx);
            err_print_all();
            
            AN *ast = parse_prog(&lx);
            
            OB ob;
            ob_init(&ob);
            codegen(&ob, ast);
            
            printf("───────────────────────────────────────────────────────────\n");
            printf("%s", ob.buf);
            printf("───────────────────────────────────────────────────────────\n");
            
            ob_free(&ob);
            afree(ast);
            lx_free(&lx);
            continue;
        }
        
        if (!strcmp(line, "hex") || !strcmp(line, ".hex")) {
            if (!srclen) {
                printf("[NHP] Buffer is empty.\n");
                continue;
            }
            
            LX lx;
            lx_init(&lx, srcbuf);
            lx_run(&lx);
            err_print_all();
            
            AN *ast = parse_prog(&lx);
            
            OB ob;
            ob_init(&ob);
            codegen(&ob, ast);
            
            hex_dump(ob.buf, ob.len);
            
            ob_free(&ob);
            afree(ast);
            lx_free(&lx);
            continue;
        }
        
        if (!strcmp(line, "tokens") || !strcmp(line, ".tokens")) {
            if (!srclen) {
                printf("[NHP] Buffer is empty.\n");
                continue;
            }
            
            LX lx;
            lx_init(&lx, srcbuf);
            lx_run(&lx);
            
            printf("\n═══ TOKEN STREAM ═══\n");
            int i;
            for (i = 0; i < lx.cnt && i < 100; i++) {
                printf("  [%3d] L%-3d Type=%-3d Val='%s'\n",
                       i, lx.t[i].line, (int)lx.t[i].type, lx.t[i].val);
            }
            if (lx.cnt > 100) printf("  ... and %d more tokens\n", lx.cnt - 100);
            printf("\n");
            
            lx_free(&lx);
            continue;
        }
        
        if (!strcmp(line, "help") || !strcmp(line, ".help")) {
            printf("\n");
            printf("╔════════════════════════════════════════════════════════════════════╗\n");
            printf("║ NHP REPL COMMANDS                                                  ║\n");
            printf("╠════════════════════════════════════════════════════════════════════╣\n");
            printf("║  run      - Compile and execute current buffer                     ║\n");
            printf("║  asm      - Show generated NASM assembly                           ║\n");
            printf("║  hex      - Show hex dump of assembly                              ║\n");
            printf("║  show     - Display current source code                            ║\n");
            printf("║  clear    - Clear source buffer                                    ║\n");
            printf("║  tokens   - Display token stream                                   ║\n");
            printf("║  help     - Show this help message                                 ║\n");
            printf("║  exit     - Exit REPL                                              ║\n");
            printf("╠════════════════════════════════════════════════════════════════════╣\n");
            printf("║ SYNTAX:                                                            ║\n");
            printf("║  Comments: ; or //                                                 ║\n");
            printf("║  Print:    Print(\"Hello World\")                                    ║\n");
            printf("║  Input:    InpuS(\"%%d\", &num)                                       ║\n");
            printf("║  If/Else:  if num >= 30 { ... } else { ... }                      ║\n");
            printf("║  Variables: My var, &var                                           ║\n");
            printf("╚════════════════════════════════════════════════════════════════════╝\n\n");
            continue;
        }
        
        // Append line to buffer
        int need = ll + 2;
        if (srclen + need >= srcap) {
            srcap *= 2;
            char *nb = (char*)realloc(srcbuf, srcap);
            if (nb) srcbuf = nb;
        }
        
        memcpy(srcbuf + srclen, line, ll);
        srclen += ll;
        srcbuf[srclen++] = '\n';
        srcbuf[srclen] = 0;
    }
    
    free(srcbuf);
    var_free();
}

/* ═══════════════════════════════════════════════════════════════════════
 * FILE I/O
 * ═══════════════════════════════════════════════════════════════════════ */

static char *readfile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "[NHP] Cannot open: %s\n", path);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    
    char *buf = (char*)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = 0;
    fclose(f);
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════
 * MAIN - Entry Point
 * ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║  NHP COMPILER v%s - ULTIMATE EDITION                              ║\n", NHP_VERSION);
    printf("║  The World's Fastest Programming Language                              ║\n");
    printf("║  Faster than Black Flash - Binary Level Optimization                   ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════╣\n");
    printf("║  Registers: SCR=rax SCT=rbx SCX=rcx SCZ=rdx SCN=r8                     ║\n");
    printf("║  Comments:  ; or //  (NOT # or -- or :)                                ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    var_init();
    
    if (argc == 1) {
        repl();
        var_free();
        return 0;
    }
    
    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        printf("USAGE:\n");
        printf("  nhpc                       = Interactive REPL\n");
        printf("  nhpc file.nhp              = Compile to NASM\n");
        printf("  nhpc file.nhp -o out.asm   = Specify output file\n");
        printf("  nhpc file.nhp -run         = Compile + execute\n");
        printf("  nhpc file.nhp -hex         = Show hex dump\n");
        printf("  nhpc file.nhp -tokens      = Show token stream\n");
        printf("  nhpc file.nhp -php         = Generate PHP code\n");
        printf("  nhpc file.nhp -v           = Verbose mode\n");
        printf("\n");
        printf("TERMUX SETUP:\n");
        printf("  pkg update && pkg upgrade\n");
        printf("  pkg install gcc nasm binutils git\n");
        printf("  gcc -O3 -march=native -o nhpc nhp_compiler_v3.c\n");
        printf("  ./nhpc hello.nhp -run\n");
        printf("\n");
        printf("EXAMPLES:\n");
        printf("  Print:     Print(\"Hello World\")\n");
        printf("  Input:     InpuS(\"%%d\", &num)\n");
        printf("  If/Else:   if num >= 30 { ... } else { ... }\n");
        printf("  Variables: My var\n");
        printf("\n");
        var_free();
        return 0;
    }
    
    const char *sf = NULL, *of = "out.asm";
    int do_run_f = 0, do_hex = 0, do_tok = 0, do_php = 0, verbose = 0;
    
    int i;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)
            of = argv[++i];
        else if (!strcmp(argv[i], "-run"))
            do_run_f = 1;
        else if (!strcmp(argv[i], "-hex"))
            do_hex = 1;
        else if (!strcmp(argv[i], "-tokens"))
            do_tok = 1;
        else if (!strcmp(argv[i], "-php"))
            do_php = 1;
        else if (!strcmp(argv[i], "-v"))
            verbose = 1;
        else if (argv[i][0] != '-')
            sf = argv[i];
    }
    
    if (!sf) {
        fprintf(stderr, "[NHP] No input file. Use -h for help.\n");
        var_free();
        return 1;
    }
    
    char *src = readfile(sf);
    if (!src) {
        var_free();
        return 1;
    }
    
    LX lx;
    lx_init(&lx, src);
    lx_run(&lx);
    
    err_print_all();
    
    if (do_tok) {
        printf("\n═══ TOKEN STREAM ═══\n");
        for (i = 0; i < lx.cnt; i++) {
            printf("  [%3d] L%-3d Type=%-3d Val='%s'\n",
                   i, lx.t[i].line, (int)lx.t[i].type, lx.t[i].val);
        }
        printf("\n");
        lx_free(&lx);
        free(src);
        var_free();
        return 0;
    }
    
    AN *ast = parse_prog(&lx);
    
    OB ob;
    ob_init(&ob);
    
    if (do_php) {
        gen_php(&ob, ast);
    } else {
        codegen(&ob, ast);
    }
    
    FILE *out = fopen(of, "w");
    if (!out) {
        fprintf(stderr, "[NHP] Cannot write %s\n", of);
        ob_free(&ob);
        afree(ast);
        lx_free(&lx);
        free(src);
        var_free();
        return 1;
    }
    
    fputs(ob.buf, out);
    fclose(out);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║ COMPILATION SUCCESSFUL                                                 ║\n");
    printf("╠════════════════════════════════════════════════════════════════════════╣\n");
    printf("║ Input:   %s%-60s║\n", sf, "                                                                                ");
    printf("║ Output:  %s%-60s║\n", of, "                                                                                ");
    printf("║ Size:    %-5d bytes%-56s║\n", ob.len, "                                                  ");
    printf("║ Tokens:  %-5d%-62s║\n", lx.cnt, "                                                     ");
    printf("║ Vars:    %-5d%-62s║\n", g_var_count, "                                                     ");
    printf("╚════════════════════════════════════════════════════════════════════════╝\n");
    
    if (verbose) {
        printf("\n[NHP] Detailed Stats:\n");
        printf("  AST Nodes: %d\n", ast->nch);
        printf("  Labels:    %d\n", lbl_ctr);
        printf("\n");
    }
    
    if (do_hex) {
        hex_dump(ob.buf, ob.len);
    }
    
    if (do_run_f) {
        do_run(ob.buf, verbose);
    } else if (!do_php) {
        printf("\n");
        printf("TO RUN:\n");
        printf("  nasm -f elf64 %s -o out.o && ld out.o -o out && ./out\n", of);
        printf("\n");
    }
    
    ob_free(&ob);
    afree(ast);
    lx_free(&lx);
    free(src);
    var_free();
    
    return 0;
}
