#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

// code is only declaration or modification expressions (variables and constants only, arrays, &*)

#define MAX_VARS_PER_STATEMENT 16
#define ID_BUFF_SIZE 64
#define STR_CONST_BUFF_SIZE 64
#define PARSER_LINE_BUFFER 1024

#define ARRAY_GROW_FACTOR 3 / 2
#define EXPRESSION_STACK_START_CAP 16
#define OPERATOR_STACK_START_CAP 16

#define SINGLE_QUOTE 0x27

#define SUCCESS 0
#define ERROR 1
#define MALLOC_ERROR 2

#define error(args...) { fprintf(stderr, "ERROR in %s:%d: ", __FILE__, __LINE__); fprintf(stderr, args); fprintf(stderr, "\n"); }
#define min(a, b) ((a) < (b) ? (a) : (b))

//#define malloc(s) (rand() % 43 == 0 ? NULL : malloc(s))
//#define calloc(n, m) (rand() % 43 == 0 ? NULL : calloc(n, m))

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef long long longlong;
typedef unsigned long long ulonglong;
typedef size_t size;

typedef enum {
    TK_END,
    TK_PRINT,
    TK_ID, TK_INT_CONSTANT, TK_FLOAT_CONSTANT, TK_STRING_CONSTANT,

    TK_EQ /* = */, 
    TK_PLUS_EQ, TK_MINUS_EQ, TK_STAR_EQ, TK_SLASH_EQ,
    TK_PERCENT_EQ, TK_LR_LR_EQ, TK_GR_GR_EQ, 
    TK_AMPERSAND_EQ, TK_PIPE_EQ, TK_CARET_EQ, 

    TK_COMMA, TK_LPAREN, TK_RPAREN, TK_LPAREN_SQ, TK_RPAREN_SQ, TK_LBR, TK_RBR,

    TK_VOID, TK_CHAR, TK_SHORT, TK_INT, TK_LONG, TK_FLOAT, TK_DOUBLE,
    TK_UNSIGNED, TK_SIGNED,

    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT, 
    TK_EQ_EQ, TK_NEQ, TK_GR, TK_LR, TK_GRE, TK_LRE,
    TK_AMPERSAND_AMPERSAND, TK_PIPE_PIPE, TK_AMPERSAND, TK_PIPE, TK_CARET,
    TK_LR_LR, TK_GR_GR,
    TK_PLUS_PLUS, TK_MINUS_MINUS,
    TK_EX_POINT, TK_TILDE,
} TokenType;

typedef struct {
    TokenType type;
    union {
        char id[ID_BUFF_SIZE];
        char str[STR_CONST_BUFF_SIZE];

        double fVal;
        longlong iVal;
    };
} Token;

typedef enum {
    PT_VOID,
    PT_CHAR, PT_UCHAR,
    PT_SHORT, PT_USHORT,
    PT_INT, PT_UINT,
    PT_LONG, PT_ULONG,
    PT_LONGLONG, PT_ULONGLONG,
    PT_FLOAT, PT_DOUBLE,

    _PT_END,
} PrimitiveType;

typedef struct {
    PrimitiveType pt;
    uint pLevel;
} Type;

typedef enum {
    OPB_ADD, OPB_SUB, OPB_MUL, OPB_DIV, OPB_MOD,
    OPB_EQ, OPB_NEQ, OPB_GR, OPB_LR, OPB_GRE, OPB_LRE,
    OPB_LAND, OPB_LOR, OPB_BAND, OPB_BOR, OPB_XOR,
    OPB_LSH, OPB_RSH, OPB_SQ_BRACKETS,

    _OPB_END,
} BinaryOperatorType;

typedef enum {
    OPU_INC = _OPB_END, OPU_DEC, OPU_PLUS, OPU_MINUS, OPU_LNOT, OPU_BNOT, OPU_ADDR_OF, OPU_PTR_DER,
    OPU_P_INC, OPU_P_DEC,
    _OPU_END,
} UnaryOperatorType;

typedef enum {
    OPA_AT = _OPU_END, OPA_ADD_AT, OPA_SUB_AT, OPA_MUL_AT, OPA_DIV_AT,
    OPA_MOD_AT, OPA_LSH_AT, OPA_RSH_AT, OPA_BAND_AT, OPA_BOR_AT, OPA_XOR_AT,
    _OPA_END,
} AssignmentOperatorType;

typedef enum {
    OPP_CAST = _OPA_END,
    _OPP_END,
} PseudoOperatorType;

typedef struct {
    uint priority : 4;
    uint isLeftAssoc: 1;
} OpPriority;

typedef struct {
    int op;
    OpPriority pr;

    union {
        Type castType;
    };
} MathToken;

typedef enum {
    ST_VARIABLE_DECLARATION, ST_EXPRESSION, ST_PRINT,
} StatementType;

typedef struct Expression Expression;

typedef struct ExpressionList {
    Expression* expr;
    struct ExpressionList* next;
} ExpressionList;

typedef enum {
    EXPR_ASSIGNMENT,
    EXPR_CAST,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_VARIABLE,
    EXPR_VALUE,
    EXPR_COMMA,
} ExpressionType;

typedef struct {
    Type type;
    union {
        char c;
        uchar uc;
        short s;
        ushort us;
        int i;
        uint ui;
        long l;
        ulong ul;
        longlong ll;
        ulonglong ull;
        double d;
        float f;

        size_t st;
        void* ptr;
    };
} ValueExpression;

typedef struct {
    char name[ID_BUFF_SIZE];
} VariableExpression;

typedef struct {
    AssignmentOperatorType op;
    Expression* expr1;
    Expression* expr2;
} AssignmentExpression;

typedef struct {
    Type type;
    Expression* expr;
} CastExpression;

typedef struct {
    UnaryOperatorType op;
    Expression* expr;
} UnaryExpression;

typedef struct {
    BinaryOperatorType op;
    Expression* expr1;
    Expression* expr2;
} BinaryExpression;

typedef struct {
    ExpressionList* exprs;
} CommaExpression;

struct Expression {
    ExpressionType type;
    union {
        AssignmentExpression ae;
        CastExpression ce;
        UnaryExpression ue;
        BinaryExpression be;
        VariableExpression ve;
        ValueExpression vle;
        CommaExpression cme;
    };
};

typedef struct { 
    char name[ID_BUFF_SIZE]; 
    int pLevel;
    int isArray;
    union {
        Expression* expr; 
        struct {
            int arraySize;
            ExpressionList* exprList;
            void* arrDataPtr;
        };
    };
} VarDeclField;

typedef struct {
    PrimitiveType vType;
    int vAmount;

    VarDeclField variables[MAX_VARS_PER_STATEMENT];

} VarDeclStatement;

typedef struct {
    Expression* expr;
} ExpressionStatement;

typedef struct {
    Expression* expr;
} PrintStatement;

typedef struct {
    StatementType type;
    char* codeLine;
    union {
        VarDeclStatement vs;
        ExpressionStatement es;
        PrintStatement ps;
    };
} Statement;

typedef struct StatementList {
    Statement st;
    struct StatementList* next;
} StatementList;

typedef struct {
    char name[ID_BUFF_SIZE];
    ValueExpression v;
} CtxVariable;

typedef struct CtxVarList {
    CtxVariable var;
    struct CtxVarList* next;
} CtxVarList;

typedef struct {
    void* regStart;
    size_t regSize;
} CtxMemoryRegion;

typedef struct CtxMemoryRegionList {
    CtxMemoryRegion region;
    struct CtxMemoryRegionList* next;
} CtxMemoryRegionList;

typedef struct {
    CtxVarList* varList;
    CtxMemoryRegionList* memRegList;
    int hasEvaluationError;
} Context;

void printValueExpression(ValueExpression* ve) {
    printf("--print-- Value: ");

    #define CASE(T, t) case PT_##T: printf("(" #t); break;
    switch (ve->type.pt) {
        CASE(VOID, void)
        CASE(CHAR, char)
        CASE(UCHAR, uchar)
        CASE(SHORT, short)
        CASE(USHORT, ushort)
        CASE(INT, int)
        CASE(UINT, uint)
        CASE(LONG, long)
        CASE(ULONG, ulong)
        CASE(LONGLONG, longlong)
        CASE(ULONGLONG, ulonglong)
        CASE(FLOAT, float)
        CASE(DOUBLE, double)
        default:
            error("Bad primitive type %d", ve->type.pt);
            return;
    }

    for (int i = 0; i < ve->type.pLevel; i++) {
        printf("*");
    }

    printf(") ");
    #undef CASE

    if (ve->type.pLevel != 0) {
        printf("%lx\n", ve->st);
        return;
    }
    switch (ve->type.pt)
    {
        case PT_VOID:
            printf("\n");
            return;
        case PT_CHAR:
            printf("%c\n", ve->c);
            return;
        case PT_UCHAR:
            printf("%u\n", (unsigned) ve->uc);
            return;
        case PT_SHORT:
            printf("%d\n", (int) ve->s);
            return;
        case PT_USHORT:
            printf("%u\n", (unsigned) ve->us);
            return;
        case PT_INT:
            printf("%d\n", ve->i);
            return;
        case PT_UINT:
            printf("%u\n", ve->ui);
            return;
        case PT_LONG:
            printf("%ld\n", ve->l);
            return;
        case PT_ULONG:
            printf("%lu\n", ve->ul);
            return;
        case PT_LONGLONG:
            printf("%lld\n", ve->ll);
            return;
        case PT_ULONGLONG:
            printf("%llu\n", ve->ull);
            return;
        case PT_FLOAT:
            printf("%f\n", ve->f);
            return;
        case PT_DOUBLE:
            printf("%lf\n", ve->d);
            return;
        
        default:
            error("Bad primitive type %d", ve->type.pt);
            return;
    }
}

char* fgetsd(char* buff, int buffSize, FILE* f, char delim) {
    char c;
    int pos = 0;
    while (pos < buffSize - 1 && fscanf(f, "%c", &c) != 0 && c != delim) {
        buff[pos++] = c;
    }
    buff[pos] = 0;
    if (pos == 0) return NULL;
    return buff;
}

void strncpy0(char* dest, const char* src, size_t n) {
    size_t i = 1;
    for (; i < n && *src; i++, src++) *(dest++) = *src;
    *dest = 0;
}

OpPriority getOpPriority(int op) {
    OpPriority r;

    switch (op) {
        case OPU_P_INC:
        case OPU_P_DEC:
        case OPB_SQ_BRACKETS:
            r.priority = 1;
            r.isLeftAssoc = 1;
            break;

        case OPU_INC:
        case OPU_DEC:
        case OPU_MINUS:
        case OPU_LNOT:
        case OPU_BNOT:
        case OPP_CAST:
        case OPU_PTR_DER:
        case OPU_ADDR_OF:
            r.priority = 2;
            r.isLeftAssoc = 0;
            break;

        case OPB_MUL:
        case OPB_DIV:
        case OPB_MOD:
            r.priority = 3;
            r.isLeftAssoc = 1;
            break;

        case OPB_ADD:
        case OPB_SUB:
            r.priority = 4;
            r.isLeftAssoc = 1;
            break;

        case OPB_LSH:
        case OPB_RSH:
            r.priority = 5;
            r.isLeftAssoc = 1;
            break;

        case OPB_LR:
        case OPB_LRE:
        case OPB_GR:
        case OPB_GRE:
            r.priority = 6;
            r.isLeftAssoc = 1;
            break;

        case OPB_EQ:
        case OPB_NEQ:
            r.priority = 7;
            r.isLeftAssoc = 1;
            break;

        case OPB_BAND:
            r.priority = 8;
            r.isLeftAssoc = 1;
            break;

        case OPB_XOR:
            r.priority = 9;
            r.isLeftAssoc = 1;
            break;

        case OPB_BOR:
            r.priority = 10;
            r.isLeftAssoc = 1;
            break;

        case OPB_LAND:
            r.priority = 11;
            r.isLeftAssoc = 1;
            break;

        case OPB_LOR:
            r.priority = 12;
            r.isLeftAssoc = 1;
            break;

        case OPA_AT:
        case OPA_ADD_AT:
        case OPA_SUB_AT:
        case OPA_MUL_AT:
        case OPA_DIV_AT:
        case OPA_MOD_AT:
        case OPA_LSH_AT:
        case OPA_RSH_AT:
        case OPA_BAND_AT:
        case OPA_XOR_AT:
        case OPA_BOR_AT:
            r.priority = 13;
            r.isLeftAssoc = 0;
            break;
    
        default:
            error("Bad operator %d", op);
            r.priority = 0;
            break;
    }
    return r;
}

int isUnary(int op) {
    return op >= OPU_INC && op < _OPU_END;
}

int isBinary(int op) {
    return op >= OPB_ADD && op < _OPB_END;
}

int isAssignment(int op) {
    return op >= OPA_AT && op < _OPA_END;
}

// Ret: -1 - error, not -1 - success
int tokenToPrefixOperator(Token* tk) {
    switch (tk->type) {
        case TK_PLUS_PLUS:
            return OPU_INC;
        case TK_MINUS_MINUS:
            return OPU_DEC;
        case TK_EX_POINT:
            return OPU_LNOT;
        case TK_TILDE:
            return OPU_BNOT;
        case TK_LPAREN:
            return OPP_CAST;
        case TK_STAR:
            return OPU_PTR_DER;
        case TK_AMPERSAND:
            return OPU_ADDR_OF;
        case TK_MINUS:
            return OPU_MINUS;
        case TK_PLUS:
            return OPU_PLUS;
        default:
            return -1;
    }
}

// Ret: -1 - error, not -1 - success
int tokenToPostfixOperator(Token* tk) {
    switch (tk->type)
    {
        case TK_PLUS_PLUS:
            return OPU_P_INC;
        case TK_MINUS_MINUS:
            return OPU_P_DEC;
        
        default:
            return -1;
    }
}

// Ret: -1 - error, not -1 - success
int tokenToBinaryOrAssignmentOperator(Token* tk) {
    switch (tk->type)
    {
        case TK_STAR:
            return OPB_MUL;
        case TK_SLASH:
            return OPB_DIV;
        case TK_PERCENT:
            return OPB_MOD;
        case TK_PLUS:
            return OPB_ADD;
        case TK_MINUS:
            return OPB_SUB;
        case TK_LR_LR:
            return OPB_LSH;
        case TK_GR_GR:
            return OPB_RSH;
        case TK_LR:
            return OPB_LR;
        case TK_GR:
            return OPB_GR;
        case TK_LRE:
            return OPB_LRE;
        case TK_GRE:
            return OPB_GRE;
        case TK_EQ_EQ:
            return OPB_EQ;
        case TK_NEQ:
            return OPB_NEQ;
        case TK_AMPERSAND:
            return OPB_BAND;
        case TK_CARET:
            return OPB_XOR;
        case TK_PIPE:
            return OPB_BOR;
        case TK_AMPERSAND_AMPERSAND:
            return OPB_LAND;
        case TK_PIPE_PIPE:
            return OPB_LOR;
        case TK_EQ:
            return OPA_AT;
        case TK_PLUS_EQ:
            return OPA_ADD_AT;
        case TK_MINUS_EQ:
            return OPA_SUB_AT;
        case TK_STAR_EQ:
            return OPA_MUL_AT;
        case TK_SLASH_EQ:
            return OPA_DIV_AT;
        case TK_PERCENT_EQ:
            return OPA_MOD_AT;
        case TK_LR_LR_EQ:
            return OPA_LSH_AT;
        case TK_GR_GR_EQ:
            return OPA_RSH_AT;
        case TK_AMPERSAND_EQ:
            return OPA_BAND_AT;
        case TK_CARET_EQ:
            return OPA_XOR_AT;
        case TK_PIPE_EQ:
            return OPA_BOR_AT;
        
        default:
            return -1;
    }
}

// skip all whitespaces
char* strskp(char* s) {
    while (isspace(*s)) s++;
    return s;
}

#define evalError(args...) { ctx->hasEvaluationError = 1; printf("Evaluation error: "); printf(args); puts(""); }

// Ret: 0 - error, not 0 - success
int sizeOf(PrimitiveType t) {
    switch (t) {
        case PT_VOID: 
            return 0;
        case PT_CHAR:
        case PT_UCHAR: return sizeof(char);
        case PT_SHORT:
        case PT_USHORT: return sizeof(short);
        case PT_INT:
        case PT_UINT: return sizeof(int);
        case PT_LONG:
        case PT_ULONG: return sizeof(long);
        case PT_LONGLONG:
        case PT_ULONGLONG: return sizeof(longlong);
        case PT_FLOAT: return sizeof(float);
        case PT_DOUBLE: return sizeof(double);
        default:
            error("bad primitive type %d", t);
            return 0;
    }
}

// Ret: 0 - error, not 0 - success
int getPointerOperationFactor(const Type* t) {
    return t->pLevel == 1 ? sizeOf(t->pt) : sizeof(size);
}

void initValueExpression(ValueExpression* v) {
    v->type.pLevel = 0;
    v->type.pt = PT_VOID;
    v->ull = 0;
}

CtxVariable* ctxGetVariable(const Context* ctx, const char* name) {
    CtxVarList* l = ctx->varList;
    for (; l != NULL; l = l->next) {
        CtxVariable* v = &l->var;
        if (!strcmp(v->name, name)) return v;
    }
    return NULL;
}

void freeCtxVarList(CtxVarList* l) {
    if (l == NULL) return;
    freeCtxVarList(l->next);
    l->next = NULL;
    free(l);
}

void ctxFreeVarList(Context* ctx) {
    freeCtxVarList(ctx->varList);
    ctx->varList = NULL;
}

void freeExpression(Expression* expr);

#define DEFINE_STACK(N, T, FREE_CODE)                           \
typedef struct {                                                \
    T* arr;                                                     \
    int size;                                                   \
    int capacity;                                               \
} Stack##N;                                                     \
                                                                \
/* Ret: MALLOC_ERROR, SUCCESS */                                \
int stackInit##N(Stack##N* st, int cap) {                       \
    T* arr = (T*) malloc(sizeof(*arr) * cap);                   \
    if (arr == NULL) return MALLOC_ERROR;                       \
    st->arr = arr;                                              \
    st->capacity = cap;                                         \
    st->size = 0;                                               \
    return SUCCESS;                                             \
}                                                               \
                                                                \
/* Ret: MALLOC_ERROR, SUCCESS */                                \
int stackPush##N(Stack##N* st, T* t) {                          \
    if (st->size == st->capacity) {                             \
        T* newArr = (T*) realloc(                               \
            st->arr,                                            \
            sizeof(*newArr) * st->size * ARRAY_GROW_FACTOR      \
        );                                                      \
        if (newArr == NULL) return MALLOC_ERROR;                \
        st->arr = newArr;                                       \
    }                                                           \
    st->arr[st->size++] = *t;                                   \
    return SUCCESS;                                             \
}                                                               \
                                                                \
/* Ret: MALLOC_ERROR, SUCCESS */                                \
int stackPop##N(Stack##N* st, T* to) {                          \
    if (st->size <= 0) return ERROR;                            \
    if (to == NULL) {                                           \
        T val;                                                  \
        val = st->arr[--st->size];                              \
        FREE_CODE;                                              \
        return ERROR;                                           \
    }                                                           \
    *to = st->arr[--st->size];                                  \
    return SUCCESS;                                             \
}                                                               \
                                                                \
/* Ret: ERROR, SUCCESS */                                       \
int stackTop##N(Stack##N* st, T* to) {                          \
    if (st->size <= 0) return ERROR;                            \
    *to = st->arr[st->size - 1];                                \
    return SUCCESS;                                             \
}                                                               \
                                                                \
void freeStack##N(Stack##N* st) {                               \
    T val;                                                      \
    while (stackPop##N(st, &val) == SUCCESS) FREE_CODE;         \
    free(st->arr);                                              \
    st->arr = NULL;                                             \
    st->size = st->capacity = 0;                                \
}

DEFINE_STACK(MathToken, MathToken, {})
DEFINE_STACK(Expression, Expression*, { freeExpression(val); })

#undef DEFINE_STACK

void freeExpressionList(ExpressionList* l) {
    if (l == NULL) return;
    freeExpressionList(l->next);

    freeExpression(l->expr);
    l->expr = NULL;
    free(l);
}

void freeStatementContent(Statement* st) {
    free(st->codeLine);
    switch (st->type)
    {
        case ST_EXPRESSION:
            freeExpression(st->es.expr);
            st->es.expr = NULL;
            break;
        case ST_VARIABLE_DECLARATION:
            for (int i = 0; i < st->vs.vAmount; i++)
            {
                VarDeclField* f = &st->vs.variables[i];
                
                if (f->isArray) {
                    freeExpressionList(f->exprList);
                    f->exprList = NULL;
                    free(f->arrDataPtr);
                    f->arrDataPtr = NULL;
                }
                else {
                    if (f->expr != NULL) {
                        freeExpression(f->expr);
                        f->expr = NULL;
                    }
                }
            }
            break;
        case ST_PRINT:
            freeExpression(st->ps.expr);
            st->ps.expr = NULL;
            break;
        default:
            error("Bad statement type %d", st->type);
            break;
    }
}

void freeStatementList(StatementList* node) {
    if (node == NULL) return;
    freeStatementList(node->next);
    node->next = NULL;

    freeStatementContent(&node->st);
    free(node);
}

void freeCtxMemRegList(CtxMemoryRegionList* node) {
    if (node == NULL) return;
    freeCtxMemRegList(node->next);
    node->next = NULL;

    free(node);
}

Expression* allocExpression(ExpressionType t) {
    Expression* expr = (Expression*) malloc(sizeof(*expr));
    if (expr == NULL) return NULL;
    expr->type = t;
    return expr;
}

StatementList* allocStatementList() {
    return (StatementList*) calloc(1, sizeof(StatementList));
}

// Ret: MALLOC_ERROR, ERROR - variable already defined, SUCCESS
int ctxRegisterVariable(Context* ctx, CtxVariable v, CtxVariable** toRetPtr) {
    if (ctxGetVariable(ctx, v.name) != NULL) return ERROR;

    CtxVarList* newNode = (CtxVarList*) malloc(sizeof(*newNode));
    if (newNode == NULL) {
        return MALLOC_ERROR;
    }
    newNode->var = v;
    newNode->next = ctx->varList;
    ctx->varList = newNode;

    if (toRetPtr != NULL)
        *toRetPtr = &newNode->var;

    return SUCCESS;
}

// Ret: MALLOC_ERROR, SUCCESS
int ctxAddMemoryRegion(Context* ctx, void* start, size_t size) {
    CtxMemoryRegionList* newNode = (CtxMemoryRegionList*) malloc(sizeof(*newNode));
    if (newNode == NULL) {
        return MALLOC_ERROR;
    }

    newNode->region.regStart = start;
    newNode->region.regSize = size;
    newNode->next = ctx->memRegList;
    ctx->memRegList = newNode;

    return SUCCESS;
}

// 1 - can, 0 - cannot
int ctxCanReadAddress(Context* ctx, void* ptr, size_t size) {
    for (CtxMemoryRegionList* node = ctx->memRegList; node; node = node->next) {
        if (ptr >= node->region.regStart && (char*) ptr + size <= (char*) node->region.regStart + node->region.regSize) return 1;
    }
    return 0;
}

// Ret: SUCCESS, ERROR
#define DEFINE_get(T) int get_##T(T* res, const ValueExpression* expr) { \
    if (expr->type.pLevel != 0) { *res = (T) expr->st; return 0; }       \
    switch (expr->type.pt) {                                             \
        case PT_VOID:                                                    \
            return ERROR;                                                \
        case PT_CHAR:                                                    \
            *res = (T) expr->c;                                          \
            return SUCCESS;                                              \
        case PT_UCHAR:                                                   \
            *res = (T) expr->uc;                                         \
            return SUCCESS;                                              \
        case PT_SHORT:                                                   \
            *res = (T) expr->s;                                          \
            return SUCCESS;                                              \
        case PT_USHORT:                                                  \
            *res = (T) expr->us;                                         \
            return SUCCESS;                                              \
        case PT_INT:                                                     \
            *res = (T) expr->i;                                          \
            return SUCCESS;                                              \
        case PT_UINT:                                                    \
            *res = (T) expr->ui;                                         \
            return SUCCESS;                                              \
        case PT_LONG:                                                    \
            *res = (T) expr->l;                                          \
            return SUCCESS;                                              \
        case PT_ULONG:                                                   \
            *res = (T) expr->ul;                                         \
            return SUCCESS;                                              \
        case PT_LONGLONG:                                                \
            *res = (T) expr->ll;                                         \
            return SUCCESS;                                              \
        case PT_ULONGLONG:                                               \
            *res = (T) expr->ull;                                        \
            return SUCCESS;                                              \
        case PT_FLOAT:                                                   \
            *res = (T) expr->f;                                          \
            return SUCCESS;                                              \
        case PT_DOUBLE:                                                  \
            *res = (T) expr->d;                                          \
            return SUCCESS;                                              \
        default:                                                         \
            error("Bad primitive type %d", expr->type.pt);               \
            return ERROR;                                                \
    }                                                                    \
}

DEFINE_get(char)
DEFINE_get(uchar)
DEFINE_get(short)
DEFINE_get(ushort)
DEFINE_get(int)
DEFINE_get(uint)
DEFINE_get(long)
DEFINE_get(ulong)
DEFINE_get(longlong)
DEFINE_get(ulonglong)
DEFINE_get(float)
DEFINE_get(double)
DEFINE_get(size)

#undef DEFINE_get

#define DEFINE_put(T) int put_##T(ValueExpression* expr, T val) {            \
    if (expr->type.pLevel != 0) { error("Cannot put to pointer"); return 1; }\
    switch (expr->type.pt) {                                                 \
        case PT_VOID:                                                        \
            return ERROR;                                                    \
        case PT_CHAR:                                                        \
            expr->c = (char) val;                                            \
            return SUCCESS;                                                  \
        case PT_UCHAR:                                                       \
            expr->uc = (uchar) val;                                          \
            return SUCCESS;                                                  \
        case PT_SHORT:                                                       \
            expr->s = (short) val;                                           \
            return SUCCESS;                                                  \
        case PT_USHORT:                                                      \
            expr->us = (ushort) val;                                         \
            return SUCCESS;                                                  \
        case PT_INT:                                                         \
            expr->i = (int) val;                                             \
            return SUCCESS;                                                  \
        case PT_UINT:                                                        \
            expr->ui = (uint) val;                                           \
            return SUCCESS;                                                  \
        case PT_LONG:                                                        \
            expr->l = (long) val;                                            \
            return SUCCESS;                                                  \
        case PT_ULONG:                                                       \
            expr->ul = (ulong) val;                                          \
            return SUCCESS;                                                  \
        case PT_LONGLONG:                                                    \
            expr->ll = (longlong) val;                                       \
            return SUCCESS;                                                  \
        case PT_ULONGLONG:                                                   \
            expr->ull = (ulonglong) val;                                     \
            return SUCCESS;                                                  \
        case PT_FLOAT:                                                       \
            expr->f = (float) val;                                           \
            return SUCCESS;                                                  \
        case PT_DOUBLE:                                                      \
            expr->c = (double) val;                                          \
            return SUCCESS;                                                  \
        default:                                                             \
            error("Bad primitive type %d", expr->type.pt);                   \
            return ERROR;                                                    \
    }                                                                        \
}

DEFINE_put(char)
DEFINE_put(uchar)
DEFINE_put(short)
DEFINE_put(ushort)
DEFINE_put(int)
DEFINE_put(uint)
DEFINE_put(long)
DEFINE_put(ulong)
DEFINE_put(longlong)
DEFINE_put(ulonglong)
DEFINE_put(float)
DEFINE_put(double)
DEFINE_put(size)

#undef DEFINE_put

#define DEFINE_ve_get(T, t, F) ValueExpression get_ve_##t(t val) {  \
    ValueExpression ve;                                             \
    ve.type.pLevel = 0;                                             \
    ve.type.pt = PT_##T;                                            \
    ve.st = 0;                                                      \
    ve. F = val;                                                    \
    return ve;                                                      \
}

DEFINE_ve_get(CHAR, char, c)
DEFINE_ve_get(UCHAR, uchar, uc)
DEFINE_ve_get(SHORT, short, s)
DEFINE_ve_get(USHORT, ushort, us)
DEFINE_ve_get(INT, int, i)
DEFINE_ve_get(UINT, uint, ui)
DEFINE_ve_get(LONG, long, l)
DEFINE_ve_get(ULONG, ulong, ul)
DEFINE_ve_get(LONGLONG, longlong, ll)
DEFINE_ve_get(ULONGLONG, ulonglong, ull)
DEFINE_ve_get(FLOAT, float, f)
DEFINE_ve_get(DOUBLE, double, d)

#undef DEFINE_ve_get

#define CASE(T, t, F) case PT_##T: {            \
    t b;                                        \
    if (get_##t(&b, src) != SUCCESS) return v;  \
    v. F = b;                                   \
    break;                                      \
}

ValueExpression castTo(Type to, const ValueExpression* src) {
    ValueExpression v;
    initValueExpression(&v);

    if (to.pLevel != 0) {
        if (src->type.pLevel != 0) {
            v.ull = src->ull;
            v.type = to;
            return v;
        }
        else if (src->type.pt == PT_FLOAT || src->type.pt == PT_DOUBLE || src->type.pt == PT_VOID) {
            return v;
        }
        else {
            if (get_size(&v.st, src) != SUCCESS) error("Error cannot be there");
            v.type = to;
            return v;
        }
    }

    switch (to.pt)
    {
        case PT_VOID:
            return v;
        CASE(CHAR, char, c)
        CASE(UCHAR, uchar, uc)
        CASE(SHORT, short, s)
        CASE(USHORT, ushort, us)
        CASE(INT, int, i)
        CASE(UINT, uint, ui)
        CASE(LONG, long, l)
        CASE(ULONG, ulong, ul)
        CASE(LONGLONG, longlong, ll)
        CASE(ULONGLONG, ulonglong, ull)
        CASE(FLOAT, float, f)
        CASE(DOUBLE, double, d)
        default:
            error("Bad primitive type %d", to.pt);
            return v;
    }
    v.type.pt = to.pt;
    return v;
}

#undef CASE

int probablyError(const ValueExpression* v) {
    return v->type.pt == PT_VOID && v->type.pLevel == 0;
}

void freeExpression(Expression* expr) {
    switch (expr->type)
    {
        case EXPR_ASSIGNMENT:
            freeExpression(expr->ae.expr1);
            expr->ae.expr1 = NULL;
            freeExpression(expr->ae.expr2);
            expr->ae.expr2 = NULL;
            break;
        case EXPR_BINARY:
            freeExpression(expr->be.expr1);
            expr->be.expr1 = NULL;
            freeExpression(expr->be.expr2);
            expr->be.expr2 = NULL;
            break;
        case EXPR_UNARY:
            freeExpression(expr->ue.expr);
            expr->ue.expr = NULL;
            break;
        case EXPR_CAST:
            freeExpression(expr->ce.expr);
            expr->ce.expr = NULL;
            break;
        case EXPR_VARIABLE:
            break;
        case EXPR_VALUE:
            break;
        case EXPR_COMMA:
            freeExpressionList(expr->cme.exprs);
            expr->cme.exprs = NULL;
            break;
        default:
            error("Bad expression type %d", expr->type);
            break;
    }
    free(expr);
}

PrimitiveType getCastType(PrimitiveType t1, PrimitiveType t2) {
    return t1 > t2 ? t1 : t2;
}

ValueExpression evaluateExpression(Context* ctx, int* changesAnyLValue, Expression* expr);

ValueExpression getLValuePtrVariable(Context* ctx, int* changesAnyLValue, const VariableExpression* expr) {
    ValueExpression v;
    CtxVariable* var = ctxGetVariable(ctx, expr->name);
    initValueExpression(&v);
    *changesAnyLValue = 0;

    if (var == NULL) {
        evalError("Cannot find variable `%s`", expr->name);
        return v;
    }

    if (var->v.type.pLevel != 0) {
        v.type = var->v.type;
        v.type.pLevel++;
        v.st = (size) &var->v.st;
        return v;
    }

    switch (var->v.type.pt)
    {
        case PT_VOID:
            evalError("Cannot get lvalue of void variable");
            return v;
        case PT_CHAR:
        case PT_UCHAR:
        case PT_SHORT:
        case PT_USHORT:
        case PT_INT:
        case PT_UINT:
        case PT_LONG:
        case PT_ULONG:
        case PT_LONGLONG:
        case PT_ULONGLONG:
        case PT_FLOAT:
        case PT_DOUBLE:
            v.st = (size) &var->v.c;
            break;
        
        default:
            error("Bad primitive type %d", var->v.type.pt);
            return v;
    }

    v.type = var->v.type;
    v.type.pLevel++;
    return v;
}

ValueExpression getLValuePtrPointerDef(Context* ctx, int* changesAnyLValue, UnaryExpression* expr) {
    ValueExpression ve = evaluateExpression(ctx, changesAnyLValue, expr->expr);

    if (ve.type.pLevel == 0) {
        ValueExpression v;
        initValueExpression(&v);

        evalError("Cannot deference non-pointer value");
        return v;
    }

    return ve;
}

ValueExpression getLValuePtrBrackets(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {
    ValueExpression ve1, ve2;
    ValueExpression v;
    size ptr, offset;
    int factor, changes = 0;

    ve1 = evaluateExpression(ctx, &changes, expr->expr1);
    if (changes) *changesAnyLValue = 1;
    changes = 0;
    ve2 = evaluateExpression(ctx, &changes, expr->expr2);
    if (changes) *changesAnyLValue = 1;

    initValueExpression(&v);

    if (ve2.type.pLevel != 0) {
        evalError("Cannot index with pointer");
        return v;
    }
    if (ve2.type.pt == PT_FLOAT || ve2.type.pt == PT_DOUBLE) {
        evalError("Cannot index with float value");
        return v;
    }
    if (ve1.type.pLevel == 0) {
        evalError("Cannot index non-pointer value");
        return v;
    }
    ptr = ve1.st;
    if (get_size(&offset, &ve2) != SUCCESS) error("There cannot be error");

    factor = getPointerOperationFactor(&ve1.type);
    if (factor == 0) return v;

    v.type = ve1.type;
    v.st = ptr + offset * factor;
    return v;
}

ValueExpression getLValuePtr(Context* ctx, int* changesAnyLValue, Expression* expr) {
    ValueExpression dflt;
    initValueExpression(&dflt);

    if (expr->type == EXPR_VARIABLE) {
        return getLValuePtrVariable(ctx, changesAnyLValue, &expr->ve);
    }
    if (expr->type == EXPR_UNARY && expr->ue.op == OPU_PTR_DER) {
        return getLValuePtrPointerDef(ctx, changesAnyLValue, &expr->ue);
    }
    if (expr->type == EXPR_BINARY && expr->be.op == OPB_SQ_BRACKETS) {
        return getLValuePtrBrackets(ctx, changesAnyLValue, &expr->be);
    }

    *changesAnyLValue = 0;
    return dflt;
}

#define CASE(T, t, F, OP1, OP2) case PT_##T:                                \
                                case PT_U##T:                               \
                                    toRet. F = OP1 (*(t*)lValuePtr.st) OP2; \
                                    break;

#define DEFINE_evaluateUnary(NAME, OP1, OP2, C1, C2)                                                \
ValueExpression evaluateUnary##NAME(Context* ctx, int* changesAnyLValue, UnaryExpression* expr) {   \
    ValueExpression toRet;                                                                          \
    ValueExpression lValuePtr = getLValuePtr(ctx, changesAnyLValue, expr->expr);                    \
    *changesAnyLValue = 1;                                                                          \
    initValueExpression(&toRet);                                                                    \
    if (probablyError(&lValuePtr)) {                                                                \
        evalError("Expression is not lvalue");                                                      \
        return toRet;                                                                               \
    }                                                                                               \
    if (lValuePtr.type.pLevel != 1) {                                                               \
        toRet.type.pLevel = lValuePtr.type.pLevel - 1;                                              \
        toRet.type.pt = lValuePtr.type.pt;                                                          \
        toRet.st = OP1 (*(size*)lValuePtr.st) OP2;                                                  \
        return toRet;                                                                               \
    }                                                                                               \
    switch (lValuePtr.type.pt) {                                                                    \
        case PT_VOID: return toRet;                                                                 \
        CASE(CHAR, char, c, OP1, OP2)                                                               \
        CASE(SHORT, short, s, OP1, OP2)                                                             \
        CASE(INT, int, i, OP1, OP2)                                                                 \
        CASE(LONG, long, l, OP1, OP2)                                                               \
        CASE(LONGLONG, longlong, ll, OP1, OP2)                                                      \
        default:                                                                                    \
            error("Bad primitive type %d", lValuePtr.type.pt);                                      \
            return toRet;                                                                           \
    }                                                                                               \
    toRet.type.pt = lValuePtr.type.pt;                                                              \
    return toRet;                                                                                   \
}

DEFINE_evaluateUnary(Inc, ++, , , toRet = *ve)
DEFINE_evaluateUnary(PInc, , ++, toRet = *ve, )
DEFINE_evaluateUnary(Dec, --, , , toRet = *ve)
DEFINE_evaluateUnary(PDec, , --, toRet = *ve, )

#undef DEFINE_evaluateUnary
#undef CASE

ValueExpression evaluateUnaryBNot(Context* ctx, int* changesAnyLValue, UnaryExpression* expr) {
    ValueExpression toRet;
    ValueExpression ve = evaluateExpression(ctx, changesAnyLValue, expr->expr);
    initValueExpression(&toRet);

    if (ve.type.pLevel != 0) {
        evalError("Cannot do ~pointer");
        return toRet;
    }

    if (ve.type.pt == PT_VOID) {
        evalError("Cannot do ~void");
        return toRet;
    }
    if (ve.type.pt == PT_FLOAT || ve.type.pt == PT_DOUBLE) {
        evalError("Cannot do ~float");
        return toRet;
    }

    ulonglong v;
    toRet.type.pt = ve.type.pt;
    if (get_ulonglong(&v, &ve) != SUCCESS) error("Cannot get ulonglong");
    if (put_ulonglong(&toRet, ~v) != SUCCESS) error("Cannot put ulonglong");
    
    return toRet;
}

ValueExpression evaluateUnaryMinus(Context* ctx, int* changesAnyLValue, UnaryExpression* expr) {
    ValueExpression ve = evaluateExpression(ctx, changesAnyLValue, expr->expr);
    ValueExpression toRet;
    initValueExpression(&toRet);

    if (ve.type.pLevel != 0) {
        evalError("Cannot calculate -pointer");
        return toRet;
    }

    switch (ve.type.pt) {
        case PT_VOID: 
            evalError("Cannot calculate -void");
            return toRet;
        case PT_CHAR:
        case PT_UCHAR: toRet.c = -ve.c; break;
        case PT_SHORT:
        case PT_USHORT: toRet.s = -ve.s; break;
        case PT_INT:
        case PT_UINT: toRet.i = -ve.i; break;
        case PT_LONG:
        case PT_ULONG: toRet.l = -ve.l; break;
        case PT_LONGLONG:
        case PT_ULONGLONG: toRet.ll = -ve.ll; break;
        case PT_FLOAT: toRet.f = -ve.f; break;
        case PT_DOUBLE: toRet.d = -ve.d; break;
        default:
            error("Bad primitive type %d", ve.type.pt);
            return toRet;
    }
    toRet.type = ve.type;
    return toRet;
}

ValueExpression getLogicalValue(Context* ctx, int* changesAnyLValue, Expression* expr) {
    ValueExpression ve = evaluateExpression(ctx, changesAnyLValue, expr);
    ValueExpression toRet;
    initValueExpression(&toRet);

    if (ve.type.pt == PT_VOID) {
        return toRet;
    }

    toRet.i = ve.ull != 0;

    toRet.type.pt = PT_INT;
    return toRet;
}

ValueExpression evaluateUnaryLNot(Context* ctx, int* changesAnyLValue, UnaryExpression* expr) {
    ValueExpression lValue = getLogicalValue(ctx, changesAnyLValue, expr->expr);

    if (probablyError(&lValue)) {
        evalError("Cannot do !void");
        return lValue;
    }

    lValue.i = !lValue.i;
    return lValue;
}

ValueExpression evaluateUnaryAddrOf(Context* ctx, int* changesAnyLValue, UnaryExpression* expr) {
    return getLValuePtr(ctx, changesAnyLValue, expr->expr);
}

ValueExpression evaluateUnaryPtrDer(Context* ctx, int* changesAnyLValue, UnaryExpression* expr) {
    ValueExpression toRet;
    ValueExpression ve = evaluateExpression(ctx, changesAnyLValue, expr->expr);
    int dSize;

    initValueExpression(&toRet);

    if (ve.type.pLevel == 0) {
        evalError("Cannot do *(non-pointer value)");
        return toRet;
    }

    if (ve.type.pLevel > 1) {
        if (!ctxCanReadAddress(ctx, (void*)ve.st, sizeof(size))) {
            evalError("Cannot access to address");
            return toRet;
        }

        toRet.type = ve.type;
        toRet.type.pLevel--;
        
        toRet.st = *(size*)ve.st;
        return toRet;
    }

    dSize = sizeOf(ve.type.pt);
    if (dSize == 0) {
        evalError("Cannot get size of void type");
        return toRet;
    }

    if (!ctxCanReadAddress(ctx, (void*)ve.st, dSize)) {
        evalError("Cannot access to address");
        return toRet;
    }

    #define CASE(T, t, F)   case PT_##T:              \
                            case PT_U##T:             \
                                toRet. F = *(t*)ve.st;\
                                break;

    switch (ve.type.pt) {
        case PT_VOID: break;
        CASE(CHAR, char, c)
        CASE(SHORT, short, s)
        CASE(INT, int, i)
        CASE(LONG, long, l)
        CASE(LONGLONG, longlong, ll)
        
        case PT_FLOAT:
            toRet.f = *(float*)ve.st;
            break;
        case PT_DOUBLE:
            toRet.d = *(double*)ve.st;
            break;

        default:
            error("Bad primitive type %d", ve.type.pt);
            break;
    }

    #undef CASE

    toRet.type.pt = ve.type.pt;
    return toRet;
}

ValueExpression evaluateUnary(Context* ctx, int* changesAnyLValue, UnaryExpression* expr) {
    switch (expr->op)
    {
        case OPU_INC: // ++i
            return evaluateUnaryInc(ctx, changesAnyLValue, expr);
        case OPU_DEC: // --i
            return evaluateUnaryDec(ctx, changesAnyLValue, expr);
        case OPU_P_INC: // i++
            return evaluateUnaryPInc(ctx, changesAnyLValue, expr);
        case OPU_P_DEC: // i--
            return evaluateUnaryPDec(ctx, changesAnyLValue, expr);
        case OPU_PLUS:
            return evaluateExpression(ctx, changesAnyLValue, expr->expr);
        case OPU_MINUS:
            return evaluateUnaryMinus(ctx, changesAnyLValue, expr);
        case OPU_LNOT: // !i
            return evaluateUnaryLNot(ctx, changesAnyLValue, expr);
        case OPU_BNOT: // ~i
            return evaluateUnaryBNot(ctx, changesAnyLValue, expr);
        case OPU_ADDR_OF: { // &i
            return evaluateUnaryAddrOf(ctx, changesAnyLValue, expr);
        }
        case OPU_PTR_DER: { // *i
            return evaluateUnaryPtrDer(ctx, changesAnyLValue, expr);
        }
        
        default: {
            ValueExpression v;
            initValueExpression(&v);
            error("bad unary operator %d", expr->op);
            return v;
        }
    }
}

ValueExpression evaluateBinaryAdd(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {
    ValueExpression v1, v2;
    ValueExpression toRet;
    PrimitiveType castPType;
    Type castType;
    int factor, changes = 0;

    v2 = evaluateExpression(ctx, &changes, expr->expr2);
    if (changes) *changesAnyLValue = 1;
    changes = 0;

    v1 = evaluateExpression(ctx, &changes, expr->expr1);
    if (changes) *changesAnyLValue = 1;

    initValueExpression(&toRet);
    
    if (v1.type.pLevel != 0) {
        size toAdd;

        if (v2.type.pLevel != 0) {
            evalError("Cannot add pointer to pointer");
            return toRet;
        }
        if (v2.type.pt == PT_VOID) return toRet;
        if (v2.type.pt == PT_FLOAT || v2.type.pt == PT_DOUBLE) {
            evalError("Cannot add float value to pointer");
            return toRet;
        }
        if (get_size(&toAdd, &v2) != SUCCESS) error("There cannot be error");

        factor = getPointerOperationFactor(&v1.type);
        if (factor == 0) return toRet;

        toRet = v1;
        toRet.st += toAdd * factor;

        return toRet;
    }
    if (v2.type.pLevel != 0) {
        size toAdd;

        if (v1.type.pt == PT_VOID) return toRet;
        if (v1.type.pt == PT_FLOAT || v1.type.pt == PT_DOUBLE) {
            evalError("Cannot add float value to pointer");
            return toRet;
        }
        if (get_size(&toAdd, &v1) != SUCCESS) error("There cannot be error");

        factor = getPointerOperationFactor(&v2.type);
        if (factor == 0) return toRet;

        toRet = v2;
        toRet.st += toAdd * factor;

        return toRet;
    }

    castPType = getCastType(v1.type.pt, v2.type.pt);
    castType.pLevel = 0;
    castType.pt = castPType;

    if (v1.type.pt != castPType) v1 = castTo(castType, &v1);
    if (v2.type.pt != castPType) v2 = castTo(castType, &v2);

    
    switch (castPType)
    {
        case PT_VOID: 
            evalError("Cannot do arithmetic operation with void");
            return toRet;
        case PT_CHAR:
        case PT_UCHAR:      toRet.uc = v1.uc + v2.uc; break;
        case PT_SHORT:
        case PT_USHORT:     toRet.us = v1.us + v2.us; break;
        case PT_INT:
        case PT_UINT:       toRet.ui = v1.ui + v2.ui; break;
        case PT_LONG:
        case PT_ULONG:      toRet.ul = v1.ul + v2.ul; break;
        case PT_LONGLONG:
        case PT_ULONGLONG:  toRet.ull = v1.ull + v2.ull; break;
        case PT_FLOAT:      toRet.f = v1.f + v2.f; break;
        case PT_DOUBLE:     toRet.d = v1.d + v2.d; break;
        default:
            error("Bad primitive type %d", castPType);
            return toRet;
    }
    toRet.type.pt = castPType;
    return toRet;
}

ValueExpression evaluateBinarySub(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {
    ValueExpression v1, v2;
    ValueExpression toRet;
    PrimitiveType castPType;
    Type castType;
    int factor, changes = 0;

    v2 = evaluateExpression(ctx, &changes, expr->expr2);
    if (changes) *changesAnyLValue = 1;
    changes = 0;

    v1 = evaluateExpression(ctx, &changes, expr->expr1);
    if (changes) *changesAnyLValue = 1;

    initValueExpression(&toRet);
    
    if (v1.type.pLevel != 0) {
        size toSub;

        if (v2.type.pLevel != 0) {
            if (v1.type.pLevel != v2.type.pLevel || v1.type.pt != v2.type.pt) {
                evalError("Cannot evaluate ptr-ptr with different pointers");
                return toRet;
            }
            toRet = v1;
            toRet.st -= v2.st;
            return toRet;
        }
        if (v2.type.pt == PT_VOID) return toRet;
        if (v2.type.pt == PT_FLOAT || v2.type.pt == PT_DOUBLE) {
            evalError("Cannot sub float value from pointer");
            return toRet;
        }
        if (get_size(&toSub, &v2) != SUCCESS) error("There cannot be error");

        factor = getPointerOperationFactor(&v1.type);
        if (factor == 0) return toRet;

        toRet = v1;
        toRet.st -= toSub * factor;

        return toRet;
    }
    if (v2.type.pLevel != 0) {
        evalError("Cannot evaluate `value - ptr`");
        return toRet;
    }

    castPType = getCastType(v1.type.pt, v2.type.pt);
    castType.pLevel = 0;
    castType.pt = castPType;

    if (v1.type.pt != castPType) v1 = castTo(castType, &v1);
    if (v2.type.pt != castPType) v2 = castTo(castType, &v2);
    
    switch (castPType)
    {
        case PT_VOID: 
            evalError("Cannot do arithmetic operation with void");
            return toRet;
        case PT_CHAR:
        case PT_UCHAR:      toRet.uc = v1.uc - v2.uc; break;
        case PT_SHORT:
        case PT_USHORT:     toRet.us = v1.us - v2.us; break;
        case PT_INT:
        case PT_UINT:       toRet.ui = v1.ui - v2.ui; break;
        case PT_LONG:
        case PT_ULONG:      toRet.ul = v1.ul - v2.ul; break;
        case PT_LONGLONG:
        case PT_ULONGLONG:  toRet.ull = v1.ull - v2.ull; break;
        case PT_FLOAT:      toRet.f = v1.f - v2.f; break;
        case PT_DOUBLE:     toRet.d = v1.d - v2.d; break;
        default:
            error("Bad primitive type %d", castPType);
            return toRet;
    }
    toRet.type.pt = castPType;
    return toRet;
}

ValueExpression evaluateBinaryMul(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {
    ValueExpression v1, v2;
    ValueExpression toRet;
    PrimitiveType castPType;
    Type castType;
    int changes = 0;

    v2 = evaluateExpression(ctx, &changes, expr->expr2);
    if (changes) *changesAnyLValue = 1;
    changes = 0;

    v1 = evaluateExpression(ctx, &changes, expr->expr1);
    if (changes) *changesAnyLValue = 1;

    initValueExpression(&toRet);
    
    if (v1.type.pLevel != 0 || v2.type.pLevel != 0) {
        evalError("Cannot do multiplication with ptr");
        return toRet;
    }

    castPType = getCastType(v1.type.pt, v2.type.pt);
    castType.pLevel = 0;
    castType.pt = castPType;

    if (v1.type.pt != castPType) v1 = castTo(castType, &v1);
    if (v2.type.pt != castPType) v2 = castTo(castType, &v2);
    
    switch (castPType)
    {
        case PT_VOID: 
            evalError("Cannot do arithmetic operation with void");
            return toRet;
        case PT_CHAR:
        case PT_UCHAR:      toRet.uc = v1.uc * v2.uc; break;
        case PT_SHORT:
        case PT_USHORT:     toRet.us = v1.us * v2.us; break;
        case PT_INT:
        case PT_UINT:       toRet.ui = v1.ui * v2.ui; break;
        case PT_LONG:
        case PT_ULONG:      toRet.ul = v1.ul * v2.ul; break;
        case PT_LONGLONG:
        case PT_ULONGLONG:  toRet.ull = v1.ull * v2.ull; break;
        case PT_FLOAT:      toRet.f = v1.f * v2.f; break;
        case PT_DOUBLE:     toRet.d = v1.d * v2.d; break;
        default:
            error("Bad primitive type %d", castPType);
            return toRet;
    }
    toRet.type.pt = castPType;
    return toRet;
}

ValueExpression evaluateBinaryDiv(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {
    ValueExpression v1, v2;
    ValueExpression toRet;
    PrimitiveType castPType;
    Type castType;
    int changes = 0;

    v2 = evaluateExpression(ctx, &changes, expr->expr2);
    if (changes) *changesAnyLValue = 1;
    changes = 0;

    v1 = evaluateExpression(ctx, &changes, expr->expr1);
    if (changes) *changesAnyLValue = 1;

    initValueExpression(&toRet);
    
    if (v1.type.pLevel != 0 || v2.type.pLevel != 0) {
        evalError("Cannot do division with ptr");
        return toRet;
    }

    castPType = getCastType(v1.type.pt, v2.type.pt);
    castType.pLevel = 0;
    castType.pt = castPType;

    if (v1.type.pt != castPType) v1 = castTo(castType, &v1);
    if (v2.type.pt != castPType) v2 = castTo(castType, &v2);
    
    switch (castPType)
    {
        case PT_VOID: 
            evalError("Cannot do arithmetic operation with void");
            return toRet;
        case PT_CHAR:       toRet.c = v1.c / v2.c; break;
        case PT_UCHAR:      toRet.uc = v1.uc / v2.uc; break;
        case PT_SHORT:      toRet.s = v1.s / v2.s; break;
        case PT_USHORT:     toRet.us = v1.us / v2.us; break;
        case PT_INT:        toRet.i = v1.i / v2.i; break;
        case PT_UINT:       toRet.ui = v1.ui / v2.ui; break;
        case PT_LONG:       toRet.l = v1.l / v2.l; break;
        case PT_ULONG:      toRet.ul = v1.ul / v2.ul; break;
        case PT_LONGLONG:   toRet.ll = v1.ll / v2.ll; break;
        case PT_ULONGLONG:  toRet.ull = v1.ull / v2.ull; break;
        case PT_FLOAT:      toRet.f = v1.f / v2.f; break;
        case PT_DOUBLE:     toRet.d = v1.d / v2.d; break;
        default:
            error("Bad primitive type %d", castPType);
            return toRet;
    }
    toRet.type.pt = castPType;
    return toRet;
}

ValueExpression evaluateBinaryMod(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {
    ValueExpression v1, v2;
    ValueExpression toRet;
    PrimitiveType castPType;
    Type castType;
    int changes = 0;

    v2 = evaluateExpression(ctx, &changes, expr->expr2);
    if (changes) *changesAnyLValue = 1;
    changes = 0;

    v1 = evaluateExpression(ctx, &changes, expr->expr1);
    if (changes) *changesAnyLValue = 1;

    initValueExpression(&toRet);
    
    if (v1.type.pLevel != 0 || v2.type.pLevel != 0) {
        evalError("Cannot evaluate modulo with ptr");
        return toRet;
    }
    if (v1.type.pt == PT_FLOAT || v1.type.pt == PT_DOUBLE ||
        v2.type.pt == PT_FLOAT || v2.type.pt == PT_DOUBLE) 
    {
        evalError("Cannot evaluate modulo with float");
        return toRet;
    }

    castPType = getCastType(v1.type.pt, v2.type.pt);
    castType.pLevel = 0;
    castType.pt = castPType;

    if (v1.type.pt != castPType) v1 = castTo(castType, &v1);
    if (v2.type.pt != castPType) v2 = castTo(castType, &v2);
    
    switch (castPType)
    {
        case PT_VOID: 
            evalError("Cannot do arithmetic operation with void");
            return toRet;
        case PT_CHAR:       toRet.c = v1.c % v2.c; break;
        case PT_UCHAR:      toRet.uc = v1.uc % v2.uc; break;
        case PT_SHORT:      toRet.s = v1.s % v2.s; break;
        case PT_USHORT:     toRet.us = v1.us % v2.us; break;
        case PT_INT:        toRet.i = v1.i % v2.i; break;
        case PT_UINT:       toRet.ui = v1.ui % v2.ui; break;
        case PT_LONG:       toRet.l = v1.l % v2.l; break;
        case PT_ULONG:      toRet.ul = v1.ul % v2.ul; break;
        case PT_LONGLONG:   toRet.ll = v1.ll % v2.ll; break;
        case PT_ULONGLONG:  toRet.ull = v1.ull % v2.ull; break;
        default:
            error("Bad primitive type %d", castPType);
            return toRet;
    }
    toRet.type.pt = castPType;
    return toRet;
}

#define DEFINE_evaluateBinary(N, OP)                                                                \
ValueExpression evaluateBinary##N(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {    \
    ValueExpression v1, v2;                                                                         \
    ValueExpression toRet;                                                                          \
    PrimitiveType castPType;                                                                        \
    Type castType;                                                                                  \
    int changes = 0;                                                                                \
                                                                                                    \
    v2 = evaluateExpression(ctx, &changes, expr->expr2);                                            \
    if (changes) *changesAnyLValue = 1;                                                             \
    changes = 0;                                                                                    \
                                                                                                    \
    v1 = evaluateExpression(ctx, &changes, expr->expr1);                                            \
    if (changes) *changesAnyLValue = 1;                                                             \
                                                                                                    \
    initValueExpression(&toRet);                                                                    \
                                                                                                    \
    if (v1.type.pLevel != 0 && v2.type.pLevel != 0) {                                               \
        if (v1.type.pLevel != v2.type.pLevel || v1.type.pt != v2.type.pt) {                         \
            evalError("Cannot compare different pointers");                                         \
            return toRet;                                                                           \
        }                                                                                           \
        toRet.type.pt = PT_INT;                                                                     \
        toRet.i = v1.st OP v2.st;                                                                   \
        return toRet;                                                                               \
    }                                                                                               \
    if ((v1.type.pLevel == 0) ^ (v2.type.pLevel == 0)) {                                            \
        evalError("Cannot compare ptr and value");                                                  \
        return toRet;                                                                               \
    }                                                                                               \
                                                                                                    \
    castPType = getCastType(v1.type.pt, v2.type.pt);                                                \
    castType.pLevel = 0;                                                                            \
    castType.pt = castPType;                                                                        \
                                                                                                    \
    if (v1.type.pt != castPType) v1 = castTo(castType, &v1);                                        \
    if (v2.type.pt != castPType) v2 = castTo(castType, &v2);                                        \
                                                                                                    \
    switch (castPType) {                                                                            \
        case PT_VOID:                                                                               \
            evalError("Cannot do logical operation with void");                                     \
            return toRet;                                                                           \
        case PT_CHAR:       toRet.i = v1.c OP v2.c; break;                                          \
        case PT_UCHAR:      toRet.i = v1.uc OP v2.uc; break;                                        \
        case PT_SHORT:      toRet.i = v1.s OP v2.s; break;                                          \
        case PT_USHORT:     toRet.i = v1.us OP v2.us; break;                                        \
        case PT_INT:        toRet.i = v1.i OP v2.i; break;                                          \
        case PT_UINT:       toRet.i = v1.ui OP v2.ui; break;                                        \
        case PT_LONG:       toRet.i = v1.l OP v2.l; break;                                          \
        case PT_ULONG:      toRet.i = v1.ul OP v2.ul; break;                                        \
        case PT_LONGLONG:   toRet.i = v1.ll OP v2.ll; break;                                        \
        case PT_ULONGLONG:  toRet.i = v1.ull OP v2.ull; break;                                      \
        case PT_FLOAT:      toRet.i = v1.f OP v2.f; break;                                          \
        case PT_DOUBLE:     toRet.i = v1.d OP v2.d; break;                                          \
        default:                                                                                    \
            error("Bad primitive type %d", castPType);                                              \
            return toRet;                                                                           \
    }                                                                                               \
    toRet.type.pt = PT_INT;                                                                         \
    return toRet;                                                                                   \
}

DEFINE_evaluateBinary(Eq, ==)
DEFINE_evaluateBinary(Neq, !=)
DEFINE_evaluateBinary(Gr, >)
DEFINE_evaluateBinary(Lr, <)
DEFINE_evaluateBinary(Gre, >=)
DEFINE_evaluateBinary(Lre, <=)

#undef DEFINE_evaluateBinary

#define DEFINE_evaluateBinary(N, OP)                                                                \
ValueExpression evaluateBinary##N(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {    \
    ValueExpression v1, v2;                                                                         \
    ValueExpression toRet;                                                                          \
    PrimitiveType castPType;                                                                        \
    Type castType;                                                                                  \
    int changes = 0;                                                                                \
                                                                                                    \
    v2 = evaluateExpression(ctx, &changes, expr->expr2);                                            \
    if (changes) *changesAnyLValue = 1;                                                             \
    changes = 0;                                                                                    \
                                                                                                    \
    v1 = evaluateExpression(ctx, &changes, expr->expr1);                                            \
    if (changes) *changesAnyLValue = 1;                                                             \
                                                                                                    \
    initValueExpression(&toRet);                                                                    \
                                                                                                    \
    if (v1.type.pLevel != 0 || v2.type.pLevel != 0) {                                               \
        evalError("Cannot bitwise with ptr");                                                       \
        return toRet;                                                                               \
    }                                                                                               \
    if (v1.type.pt == PT_FLOAT || v1.type.pt == PT_DOUBLE ||                                        \
        v2.type.pt == PT_FLOAT || v2.type.pt == PT_DOUBLE)                                          \
    {                                                                                               \
        evalError("Cannot bitwise with float");                                                     \
        return toRet;                                                                               \
    }                                                                                               \
                                                                                                    \
    castPType = getCastType(v1.type.pt, v2.type.pt);                                                \
    castType.pLevel = 0;                                                                            \
    castType.pt = castPType;                                                                        \
                                                                                                    \
    if (v1.type.pt != castPType) v1 = castTo(castType, &v1);                                        \
    if (v2.type.pt != castPType) v2 = castTo(castType, &v2);                                        \
                                                                                                    \
    if (castPType < PT_VOID || castPType > PT_ULONGLONG) {                                          \
        error("Bad primitive type %d", castPType);                                                  \
        return toRet;                                                                               \
    }                                                                                               \
    if (castPType == PT_VOID) {                                                                     \
        evalError("Cannot do bitwise operation with void");                                         \
        return toRet;                                                                               \
    }                                                                                               \
    toRet.st = v1.st OP v2.st;                                                                      \
    toRet.type.pt = castPType;                                                                      \
    return toRet;                                                                                   \
}

DEFINE_evaluateBinary(Band, &)
DEFINE_evaluateBinary(Bor, |)
DEFINE_evaluateBinary(Xor, ^)

#undef DEFINE_evaluateBinary

#define DEFINE_evaluateBinary(N, OP)                                                                \
ValueExpression evaluateBinary##N(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {    \
    ValueExpression v1, v2;                                                                         \
    int changes = 0;                                                                                \
    v2 = getLogicalValue(ctx, &changes, expr->expr2);                                               \
    if (changes) *changesAnyLValue = 1;                                                             \
    if (probablyError(&v2)) {                                                                       \
        evalError("Cannot do " #OP " with void");                                                   \
        return v1;                                                                                  \
    }                                                                                               \
    changes = 0;                                                                                    \
    v1 = getLogicalValue(ctx, &changes, expr->expr1);                                               \
    if (changes) *changesAnyLValue = 1;                                                             \
    if (probablyError(&v1)) {                                                                       \
        evalError("Cannot do " #OP " with void");                                                   \
        return v2;                                                                                  \
    }                                                                                               \
    v1.i = v1.i OP v2.i;                                                                            \
    return v1;                                                                                      \
}

DEFINE_evaluateBinary(Land, &&)
DEFINE_evaluateBinary(Lor, ||)

#undef DEFINE_evaluateBinary

#define DEFINE_evaluateBinary(N, OP)                                                                \
ValueExpression evaluateBinary##N(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {    \
    ValueExpression v1, v2;                                                                         \
    ValueExpression toRet;                                                                          \
    PrimitiveType castPType;                                                                        \
    Type castType;                                                                                  \
    int changes = 0;                                                                                \
                                                                                                    \
    v2 = evaluateExpression(ctx, &changes, expr->expr2);                                            \
    if (changes) *changesAnyLValue = 1;                                                             \
    changes = 0;                                                                                    \
                                                                                                    \
    v1 = evaluateExpression(ctx, &changes, expr->expr1);                                            \
    if (changes) *changesAnyLValue = 1;                                                             \
                                                                                                    \
    initValueExpression(&toRet);                                                                    \
                                                                                                    \
    if (v1.type.pLevel != 0 || v2.type.pLevel != 0) {                                               \
        evalError("Cannot bitwise with ptr");                                                       \
        return toRet;                                                                               \
    }                                                                                               \
    if (v1.type.pt == PT_FLOAT || v1.type.pt == PT_DOUBLE ||                                        \
        v2.type.pt == PT_FLOAT || v2.type.pt == PT_DOUBLE)                                          \
    {                                                                                               \
        evalError("Cannot bitwise with float");                                                     \
        return toRet;                                                                               \
    }                                                                                               \
                                                                                                    \
    castPType = getCastType(v1.type.pt, v2.type.pt);                                                \
    castType.pLevel = 0;                                                                            \
    castType.pt = castPType;                                                                        \
                                                                                                    \
    if (v1.type.pt != castPType) v1 = castTo(castType, &v1);                                        \
    if (v2.type.pt != castPType) v2 = castTo(castType, &v2);                                        \
                                                                                                    \
    if (castPType == PT_VOID) {                                                                     \
        evalError("Cannot do bitwise operation with void");                                         \
        return toRet;                                                                               \
    }                                                                                               \
                                                                                                    \
    switch (castPType) {                                                                            \
        case PT_CHAR:                                                                               \
        case PT_UCHAR:                                                                              \
            toRet.c = v1.c OP v2.c;                                                                 \
            break;                                                                                  \
        case PT_SHORT:                                                                              \
            toRet.s = v1.s OP v2.s;                                                                 \
            break;                                                                                  \
        case PT_USHORT:                                                                             \
            toRet.us = v1.us OP v2.us;                                                              \
            break;                                                                                  \
        case PT_INT:                                                                                \
            toRet.i = v1.i OP v2.i;                                                                 \
            break;                                                                                  \
        case PT_UINT:                                                                               \
            toRet.ui = v1.ui OP v2.ui;                                                              \
            break;                                                                                  \
        case PT_LONG:                                                                               \
            toRet.ul = v1.ul OP v2.ul;                                                              \
            break;                                                                                  \
        case PT_ULONG:                                                                              \
            toRet.l = v1.l OP v2.l;                                                                 \
            break;                                                                                  \
        case PT_LONGLONG:                                                                           \
            toRet.ll = v1.ll OP v2.ll;                                                              \
            break;                                                                                  \
        case PT_ULONGLONG:                                                                          \
            toRet.ull = v1.ull OP v2.ull;                                                           \
            break;                                                                                  \
                                                                                                    \
        default:                                                                                    \
            error("Bad primitive type %d", castPType);                                              \
            return toRet;                                                                           \
    }                                                                                               \
                                                                                                    \
    toRet.type.pt = castPType;                                                                      \
    return toRet;                                                                                   \
}

DEFINE_evaluateBinary(Lsh, <<)
DEFINE_evaluateBinary(Rsh, >>)

#undef DEFINE_evaluateBinary

ValueExpression evaluateSquareBrackets(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {
    ValueExpression whatVe, offsetVe, toRet;
    size offset, ptr;
    int changes = 0, dSize;

    initValueExpression(&toRet);

    whatVe = evaluateExpression(ctx, &changes, expr->expr1);
    if (changes) *changesAnyLValue = 1;
    changes = 0;

    if (whatVe.type.pLevel == 0) {
        evalError("Cannot index non-ptr type");
        return toRet;
    }
    
    offsetVe = evaluateExpression(ctx, &changes, expr->expr2);
    if (changes) *changesAnyLValue = 1;

    if (offsetVe.type.pLevel != 0) {
        evalError("Cannot index with ptr");
        return toRet;
    }

    if (offsetVe.type.pt == PT_VOID) {
        evalError("Cannot index with void");
        return toRet;
    }

    if (offsetVe.type.pt == PT_FLOAT || offsetVe.type.pt == PT_DOUBLE) {
        evalError("Cannot index with float");
        return toRet;
    }

    if (get_size(&offset, &offsetVe) != SUCCESS) error("Error cannot be there");
    if (get_size(&ptr, &whatVe) != SUCCESS) error("Error cannot be there");

    ptr += offset * getPointerOperationFactor(&whatVe.type);

    if (whatVe.type.pLevel >= 2) {
        if (!ctxCanReadAddress(ctx, (void*)ptr, sizeof(size))) {
            evalError("Cannot access to address");
            return toRet;
        }

        toRet.type.pt = whatVe.type.pt;
        toRet.type.pLevel = whatVe.type.pLevel - 1;
        toRet.st = *(size*)ptr;
        return toRet;
    }

    dSize = sizeOf(whatVe.type.pt);
    if (dSize == 0) {
        evalError("Cannot get size of void type");
        return toRet;
    }

    if (!ctxCanReadAddress(ctx, (void*)ptr, dSize)) {
        evalError("Cannot access to address");
        return toRet;
    }

    #define CASE(T, t, F) case PT_##T: case PT_U##T: toRet. F = *(t*)ptr; break;

    switch (whatVe.type.pt)
    {
        case PT_VOID:
            evalError("Cannot index `void*`");
            return toRet;
        CASE(CHAR, char, c)
        CASE(SHORT, short, s)
        CASE(INT, int, i)
        CASE(LONG, long, l)
        CASE(LONGLONG, longlong, ll)
        case PT_FLOAT:
            toRet.f = *(float*)ptr;
            break;
        case PT_DOUBLE:
            toRet.d = *(double*)ptr;
            break;
        
        default:
            error("Bad primitive type %d", whatVe.type.pt);
            return toRet;
    }

    #undef CASE

    toRet.type.pt = whatVe.type.pt;
    return toRet;
}

ValueExpression evaluateBinary(Context* ctx, int* changesAnyLValue, BinaryExpression* expr) {
    switch (expr->op)
    {
        case OPB_ADD:
            return evaluateBinaryAdd(ctx, changesAnyLValue, expr);
        case OPB_SUB:
            return evaluateBinarySub(ctx, changesAnyLValue, expr);
        case OPB_MUL:
            return evaluateBinaryMul(ctx, changesAnyLValue, expr);
        case OPB_DIV:
            return evaluateBinaryDiv(ctx, changesAnyLValue, expr);
        case OPB_MOD:
            return evaluateBinaryMod(ctx, changesAnyLValue, expr);
        case OPB_EQ:
            return evaluateBinaryEq(ctx, changesAnyLValue, expr);
        case OPB_NEQ:
            return evaluateBinaryNeq(ctx, changesAnyLValue, expr);
        case OPB_GR:
            return evaluateBinaryGr(ctx, changesAnyLValue, expr);
        case OPB_LR:
            return evaluateBinaryLr(ctx, changesAnyLValue, expr);
        case OPB_GRE:
            return evaluateBinaryGre(ctx, changesAnyLValue, expr);
        case OPB_LRE:
            return evaluateBinaryLre(ctx, changesAnyLValue, expr);
        case OPB_LAND:
            return evaluateBinaryLand(ctx, changesAnyLValue, expr);
        case OPB_LOR:
            return evaluateBinaryLor(ctx, changesAnyLValue, expr);
        case OPB_BAND:
            return evaluateBinaryBand(ctx, changesAnyLValue, expr);
        case OPB_BOR:
            return evaluateBinaryBor(ctx, changesAnyLValue, expr);
        case OPB_XOR:
            return evaluateBinaryXor(ctx, changesAnyLValue, expr);
        case OPB_LSH:
            return evaluateBinaryLsh(ctx, changesAnyLValue, expr);
        case OPB_RSH:
            return evaluateBinaryRsh(ctx, changesAnyLValue, expr);
        case OPB_SQ_BRACKETS:
            return evaluateSquareBrackets(ctx, changesAnyLValue, expr);
        
        default: {
            ValueExpression v;
            initValueExpression(&v);
            error("Bad binary operator %d", expr->op);
            return v;
        }   
    }
}

ValueExpression evaluateAT(Context* ctx, int* changesAnyLValue, AssignmentExpression* expr) {
    ValueExpression to, what, voidRet;
    Type castType;
    int changes = 0, dSize;

    initValueExpression(&voidRet);

    what = evaluateExpression(ctx, &changes, expr->expr2);
    if (changes) *changesAnyLValue = 1;

    if (probablyError(&what)) {
        return what;
    }

    to = getLValuePtr(ctx, &changes, expr->expr1);
    if (changes) *changesAnyLValue = 1;

    if (probablyError(&to)) {
        return to;
    }

    castType = to.type;
    castType.pLevel--;

    what = castTo(castType, &what);

    if (castType.pLevel != 0) {
        if (!ctxCanReadAddress(ctx, (void*)to.st, sizeof(size))) {
            evalError("Cannot access to address");
            return voidRet;
        }
        *(size*)to.st = what.st;
        return what;
    }

    dSize = sizeOf(to.type.pt);
    if (dSize == 0) {
        evalError("Cannot get size of void type");
        return voidRet;
    }

    if (!ctxCanReadAddress(ctx, (void*)to.st, dSize)) {
        evalError("Cannot access to address");
        return voidRet;
    }

    #define CASE(T, t, F)   case PT_##T:                                        \
                            case PT_U##T:                                       \
                                if (!*changesAnyLValue)                         \
                                    *changesAnyLValue = *(t*)to.st != what. F;  \
                                *(t*)to.st = what. F;                           \
                                break;

    switch (to.type.pt) {
        CASE(CHAR, char, c)
        CASE(SHORT, short, s)
        CASE(INT, int, i)
        CASE(LONG, long, l)
        CASE(LONGLONG, longlong, ll)
        case PT_FLOAT:
            if (!*changesAnyLValue)
                *changesAnyLValue = *(float*)to.st != what.f;
            *(float*)to.st = what.f;
            break;
        case PT_DOUBLE:
            if (!*changesAnyLValue)
                *changesAnyLValue = *(double*)to.st != what.d;
            *(double*)to.st = what.d;
            break;
        
        default:
            error("Bad primitive type %d", to.type.pt)
            return voidRet;
    }

    #undef CASE
    return what;
}

#define PASTE(OP)                                                               \
    {int changes = 0;                                                           \
    Expression binary, newAt, unpack, vPtr;                                     \
    ValueExpression toRet, lvPtr = getLValuePtr(ctx, &changes, expr->expr1);    \
    if (probablyError(&lvPtr)) {                                                \
        evalError("Cannot get lvalue");                                         \
        return lvPtr;                                                           \
    };                                                                          \
    if (changes) *changesAnyLValue = 1;                                         \
    vPtr.type = EXPR_VALUE;                                                     \
    vPtr.vle = lvPtr;                                                           \
    unpack.type = EXPR_UNARY;                                                   \
    unpack.ue.op = OPU_PTR_DER;                                                 \
    unpack.ue.expr = &vPtr;                                                     \
    binary.type = EXPR_BINARY;                                                  \
    binary.be.op = OP;                                                          \
    binary.be.expr1 = &unpack;                                                  \
    binary.be.expr2 = expr->expr2;                                              \
    newAt.type = EXPR_ASSIGNMENT;                                               \
    newAt.ae.op = OPA_AT;                                                       \
    newAt.ae.expr1 = &unpack;                                                   \
    newAt.ae.expr2 = &binary;                                                   \
    changes = 0;                                                                \
    toRet = evaluateAT(ctx, &changes, &newAt.ae);                               \
    if (changes) *changesAnyLValue = 1;                                         \
    return toRet;}

ValueExpression evaluateAssignment(Context* ctx, int* changesAnyLValue, AssignmentExpression* expr) {
    switch (expr->op) {
        case OPA_AT:
            return evaluateAT(ctx, changesAnyLValue, expr);
        case OPA_ADD_AT: 
            PASTE(OPB_ADD);
        case OPA_SUB_AT:
            PASTE(OPB_SUB);
        case OPA_MUL_AT:
            PASTE(OPB_MUL);
        case OPA_DIV_AT:
            PASTE(OPB_DIV);
        case OPA_MOD_AT:
            PASTE(OPB_MOD);
        case OPA_LSH_AT:
            PASTE(OPB_LSH);
        case OPA_RSH_AT:
            PASTE(OPB_RSH);
        case OPA_BAND_AT:
            PASTE(OPB_BAND);
        case OPA_BOR_AT:
            PASTE(OPB_BOR);
        case OPA_XOR_AT:
            PASTE(OPB_XOR);
        default: {
            ValueExpression v;
            initValueExpression(&v);
            error("Bad assignment operator %d", expr->op);
            return v;
        }  
    }
}

#undef PASTE

ValueExpression evaluateExpression(Context* ctx, int* changesAnyValue, Expression* expr) {
    switch (expr->type)
    {
        case EXPR_ASSIGNMENT:
            return evaluateAssignment(ctx, changesAnyValue, &expr->ae);
        case EXPR_BINARY:
            return evaluateBinary(ctx, changesAnyValue, &expr->be);
        case EXPR_UNARY:
            return evaluateUnary(ctx, changesAnyValue, &expr->ue);
        case EXPR_CAST: {
            ValueExpression ve = evaluateExpression(ctx, changesAnyValue, expr->ce.expr);
            ve = castTo(expr->ce.type, &ve);
            if (probablyError(&ve)) {
                evalError("Cannot cast types");
            }
            return ve;
        }
        case EXPR_VARIABLE: {
            CtxVariable* var = ctxGetVariable(ctx, expr->ve.name);
            if (var == NULL) {
                ValueExpression v;
                initValueExpression(&v);

                evalError("Cannot find variable `%s`", expr->ve.name);
                return v;
            }
            *changesAnyValue = 0;
            return var->v;
        }
        case EXPR_VALUE:
            *changesAnyValue = 0;
            return expr->vle;
        case EXPR_COMMA:
            *changesAnyValue = 0;

            for (ExpressionList* node = expr->cme.exprs; ; node = node->next) {
                int changes = 0;
                if (node->next != NULL) {
                    evaluateExpression(ctx, &changes, node->expr);
                    if (changes) *changesAnyValue = 1;
                }
                else {
                    ValueExpression ve = evaluateExpression(ctx, &changes, node->expr);
                    if (changes) *changesAnyValue = 1;

                    return ve;
                }
            }
            error("Error cannot be there");
        default: {
            ValueExpression v;
            initValueExpression(&v);
            *changesAnyValue = 0;
            error("Cannot evaluate expression type %d", expr->type);
            return v;
        }   
    }
}

// Ret: ERROR, MALLOC_ERROR, SUCCESS
int interpretStatement(int* fChanges, Context* ctx, Statement* statement) {
    ctx->hasEvaluationError = 0;

    switch (statement->type)
    {
        case ST_VARIABLE_DECLARATION:
            for (int i = 0; i < statement->vs.vAmount; i++) {
                VarDeclField* f = &statement->vs.variables[i];
                int j, regStatus, factor, dSize;
                CtxVariable var,* registeredVarPtr;
                Type declType;
                ValueExpression ve;
                ExpressionList* node;

                initValueExpression(&ve);
                declType.pLevel = f->pLevel;
                declType.pt = statement->vs.vType;

                if (declType.pt == PT_VOID && declType.pLevel == 0) {
                    evalError("Cannot create variable with void type");
                    return ERROR;
                }

                if (f->isArray) {
                    void* ptr; 
                    int arrSize;

                    ve.type = declType;
                    ve.type.pLevel++;

                    factor = getPointerOperationFactor(&ve.type);
                    if (factor == 0) {
                        evalError("Cannot get size of array type");
                        return ERROR;
                    }

                    arrSize = f->arraySize;
                    if (arrSize == 0) {
                        for (node = f->exprList; node != NULL; node = node->next) arrSize++;
                    }

                    ptr = malloc(factor * arrSize);
                    if (ptr == NULL) {
                        error("Cannot allocate array for variable");
                        return MALLOC_ERROR;
                    }
                    f->arrDataPtr = ptr;

                    if (ctxAddMemoryRegion(ctx, ptr, arrSize * factor) != SUCCESS) {
                        error("Memory allocation error");
                        free(ptr);
                        f->arrDataPtr = NULL;
                        return MALLOC_ERROR;
                    }

                    for (j = 0, node = f->exprList; node != NULL; j++, node = node->next) {
                        ValueExpression evaluated;
                        if (j >= arrSize) {
                            evalError("Too many expressions in array");
                            free(ptr);
                            f->arrDataPtr = NULL;
                            return ERROR;
                        }

                        evaluated = evaluateExpression(ctx, fChanges, node->expr);

                        if (evaluated.type.pt != declType.pt || evaluated.type.pLevel != declType.pLevel) {
                            evaluated = castTo(declType, &evaluated);
                            if (probablyError(&evaluated)) {
                                evalError("Cannot cast evaluated value to array type");
                                free(ptr);
                                f->arrDataPtr = NULL;
                                return ERROR;
                            }
                        }

                        if (evaluated.type.pLevel != 0) {
                            *((size*)ptr + j) = evaluated.st;
                            continue;
                        }

                        #define CASE(T, t, F)   case PT_##T:                        \
                                                case PT_U##T:                       \
                                                    *((t*)ptr + j) = evaluated. F;  \
                                                    break;

                        switch (evaluated.type.pt)
                        {
                            case PT_VOID:
                                evalError("Cannot assign void value to array element");
                                return ERROR;
                            CASE(CHAR, char, c)
                            CASE(SHORT, short, s)
                            CASE(INT, int, i)
                            CASE(LONG, long, l)
                            CASE(LONGLONG, longlong, ll)
                            case PT_FLOAT:
                                *((float*)ptr + j) = evaluated.f;
                            case PT_DOUBLE:
                                *((double*)ptr + j) = evaluated.d;
                            default:
                                error("Bad primitive type %d", evaluated.type.pt);
                                return ERROR;
                        }

                        #undef CASE
                    }
                    
                    ve.st = (size) ptr;
                }
                else {
                    Expression* expr = f->expr;
                    ve.type = declType;

                    if (expr != NULL) {
                        ve = evaluateExpression(ctx, fChanges, expr);
                        if (declType.pt != ve.type.pt || declType.pLevel != ve.type.pLevel) {
                            ve = castTo(declType, &ve);
                            if (probablyError(&ve)) {
                                evalError("Cannot assign value to variable");
                                return ERROR;
                            }
                        }
                    }
                }
                var.v = ve;
                strcpy(var.name, f->name);
                regStatus = ctxRegisterVariable(ctx, var, &registeredVarPtr);
                if (regStatus == MALLOC_ERROR) {
                    error("Memory allocation error");
                    return MALLOC_ERROR;
                }

                dSize = sizeOf(var.v.type.pt);
                if (dSize == 0) {
                    evalError("Cannot get size of void type");
                    return ERROR;
                }

                if (ctxAddMemoryRegion(ctx, &registeredVarPtr->v.c, 
                                       var.v.type.pLevel != 0 ? sizeof(size_t) : dSize
                                      ) != SUCCESS) {
                    error("Memory allocation error");
                    return MALLOC_ERROR;
                }
                
                if (regStatus == ERROR) {
                    evalError("Cannot register variable `%s`", f->name);
                    return ERROR;
                }
            }
            *fChanges = 1;
            break;
        case ST_EXPRESSION:
            evaluateExpression(ctx, fChanges, statement->es.expr);
            break;
        case ST_PRINT: {
            ValueExpression ve = evaluateExpression(ctx, fChanges, statement->ps.expr);
            printValueExpression(&ve);
            break;
        }
        default:
            error("Bad statement type %d", statement->type);
            return ERROR;
    }

    if (ctx->hasEvaluationError) return ERROR;
    return SUCCESS;
}

#undef evalError

// Ret: NULL - no token detected, next s - success
char* parseToken(Token* to, char* s) {
    char* begin;
    s = strskp(s);
    begin = s;

    if (*s == 0) to->type = TK_END;
    else if (isalpha(*s) || *s == '_') {
        size l;
        char* st = s;
        s++;
        while (isalpha(*s) || *s == '_' || isdigit(*s)) s++;

        strncpy0(to->id, st, min(s - st + 1, ID_BUFF_SIZE));

        if (strcmp(to->id, "void") == 0) to->type = TK_VOID;
        else if (strcmp(to->id, "char") == 0) to->type = TK_CHAR;
        else if (strcmp(to->id, "short") == 0) to->type = TK_SHORT;
        else if (strcmp(to->id, "int") == 0) to->type = TK_INT;
        else if (strcmp(to->id, "long") == 0) to->type = TK_LONG;
        else if (strcmp(to->id, "float") == 0) to->type = TK_FLOAT;
        else if (strcmp(to->id, "double") == 0) to->type = TK_DOUBLE;
        else if (strcmp(to->id, "signed") == 0) to->type = TK_SIGNED;
        else if (strcmp(to->id, "unsigned") == 0) to->type = TK_UNSIGNED;
        else if (strcmp(to->id, "print") == 0) to->type = TK_PRINT;
        else to->type = TK_ID;
    }
    else if (isdigit(*s) || *s == '.') {
        char buff[ID_BUFF_SIZE];
        char* start = s;
        int isFloat = *s == '.';
        
        if (*s == '0' && *(s + 1) == 'x') {
            start = s += 2;
            while (isxdigit(*s)) s++;

            to->type = TK_INT_CONSTANT;
            strncpy0(buff, start, min(s - start + 1, sizeof(buff)));
            to->iVal = strtoll(buff, NULL, 16);
        }
        else {
            s++;
            while (isdigit(*s)) s++;
            if (!isFloat && *s == '.') s++, isFloat = 1;
            while (isdigit(*s)) s++;

            strncpy0(buff, start, min(s - start + 1, sizeof(buff)));
            if (isFloat) {
                to->type = TK_FLOAT_CONSTANT;
                to->fVal = atof(buff);
            }
            else {
                to->type = TK_INT_CONSTANT;
                to->iVal = atoll(buff);
            }
        }
    }
    else {
        switch(*s) {
            case SINGLE_QUOTE:
                // TODO: parse char
                error("Not implemented");
                break;
            case '"':
                // TODO: parse string
                error("Not implemented");
                break;
            case '+':
                s++;
                if (*s == '+') {
                    s++;
                    to->type = TK_PLUS_PLUS;
                }
                else if (*s == '=') {
                    s++;
                    to->type = TK_PLUS_EQ;
                }
                else to->type = TK_PLUS;
                break;
            case '-':
                s++;
                if (*s == '-') {
                    s++;
                    to->type = TK_MINUS_MINUS;
                }
                else if (*s == '=') {
                    s++;
                    to->type = TK_MINUS_EQ;
                }
                else to->type = TK_MINUS;
                break;
            case '*':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_STAR_EQ;
                }
                else to->type = TK_STAR;
                break;
            case '/':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_SLASH_EQ;
                }
                else to->type = TK_SLASH;
                break;
            case '%':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_PERCENT_EQ;
                }
                else to->type = TK_PERCENT;
                break;
            case '&':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_AMPERSAND_EQ;
                }
                else if (*s == '&') {
                    s++;
                    to->type = TK_AMPERSAND_AMPERSAND;
                }
                else to->type = TK_AMPERSAND;
                break;
            case '|':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_PIPE_EQ;
                }
                else if (*s == '|') {
                    s++;
                    to->type = TK_PIPE_PIPE;
                }
                else to->type = TK_PIPE;
                break;
            case '=':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_EQ_EQ;
                }
                else to->type = TK_EQ;
                break;
            case '!':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_NEQ;
                }
                else to->type = TK_EX_POINT;
                break;
            case '~':
                s++;
                to->type = TK_TILDE;
                break;
            case '<':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_LRE;
                }
                else if (*s == '<') {
                    s++;
                    if (*s == '=') {
                        s++;
                        to->type = TK_LR_LR_EQ;
                    }
                    else {
                        to->type = TK_LR_LR;
                    }
                }
                else to->type = TK_LR;
                break;
            case '>':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_GRE;
                }
                else if (*s == '>') {
                    s++;
                    if (*s == '=') {
                        s++;
                        to->type = TK_GR_GR_EQ;
                    }
                    else {
                        to->type = TK_GR_GR;
                    }
                }
                else to->type = TK_GR;
                break;
            case '^':
                s++;
                if (*s == '=') {
                    s++;
                    to->type = TK_CARET_EQ;
                }
                else to->type = TK_CARET;
                break;
            case ',':
                s++;
                to->type = TK_COMMA;
                break;
            case '(':
                s++;
                to->type = TK_LPAREN;
                break;
            case ')':
                s++;
                to->type = TK_RPAREN;
                break;
            case '[':
                s++;
                to->type = TK_LPAREN_SQ;
                break;
            case ']':
                s++;
                to->type = TK_RPAREN_SQ;
                break;
            case '{':
                s++;
                to->type = TK_LBR;
                break;
            case '}':
                s++;
                to->type = TK_RBR;
                break;

            default:
                return NULL;
        }
        strncpy0(to->id, begin, s - begin + 1);
    }

    return s;
}

#define next() st = parseToken(tk, st), st != NULL
#define match(t) (tk->type == t ? next(), 1 : 0)
#define is(t) (tk->type == t)
#define parserError(args...) { printf("Parser error: "); printf(args); puts(""); }
#define errExp(T) parserError("Expected " #T)

int parseExpression(Token* tk, Expression** toE, char* st, char** toS);

int isPrefixUnaryToken(Token* tk) {
    return is(TK_PLUS_PLUS) || is(TK_MINUS_MINUS) || is(TK_PLUS) || is(TK_MINUS) ||
            is(TK_EX_POINT) || is(TK_TILDE) || is(TK_STAR) || is(TK_AMPERSAND);
}

int isPostfixUnaryToken(Token* tk) {
    return is(TK_PLUS_PLUS) || is(TK_MINUS_MINUS);
}

int isTypeBeginning(Token* tk) {
    return is(TK_UNSIGNED) || is(TK_SIGNED) || is(TK_VOID) || is(TK_CHAR) || 
        is(TK_SHORT) || is(TK_INT) || is(TK_LONG) || is(TK_FLOAT) || is(TK_DOUBLE);
}

// Ret: NULL is error, next s if success
char* parseDeclarationType(Token* tk, PrimitiveType* to, char* st) {
    struct {
        uint isChar : 1;
        uint isShort : 1;
        uint isFloat : 1;
        uint isDouble : 1;
        uint isVoid : 1;
    } t;

    static_assert(sizeof(t) == sizeof(int), "Need change type");

    int* ti = (int*) &t;
    int cntLong = 0;
    int isUnsigned = 0;
    int isSigned = 0;
    int isInt = 0;

    *ti = 0;

    for (;;) {
        if (match(TK_VOID)) { if (*ti) return NULL; t.isVoid = 1; }
        else if (match(TK_CHAR)) { if (*ti) return NULL; t.isChar = 1; }
        else if (match(TK_SHORT)) { if (*ti) return NULL; t.isShort = 1; }
        else if (match(TK_INT)) { if (t.isChar || t.isFloat || t.isDouble || isInt) return NULL; isInt = 1; }

        else if (match(TK_FLOAT)) { if (*ti || isInt) return NULL; t.isFloat = 1; }
        else if (match(TK_DOUBLE)) { if (*ti || isInt) return NULL; t.isDouble = 1; }

        else if (match(TK_UNSIGNED)) { if (isUnsigned || isSigned) return NULL; isUnsigned = 1; }
        else if (match(TK_SIGNED)) { if (isSigned || isUnsigned) return NULL; isSigned = 1; }

        else if (match(TK_LONG)) cntLong++;
        else break;
    }

    if (t.isVoid) {
        if (cntLong != 0) return NULL;
        if (isUnsigned != 0) return NULL;
        if (isSigned != 0) return NULL;
        if (isInt != 0) return NULL;

        *to = PT_VOID;
        return st;
    }
    if (t.isChar) {
        if (cntLong != 0) return NULL;
        if (isUnsigned) {
            *to = PT_UCHAR;
            return st;
        }
        *to = PT_CHAR;
        return st;
    }
    if (t.isShort) {
        if (cntLong != 0) return NULL;
        if (isUnsigned) {
            *to = PT_USHORT;
            return st;
        }
        *to = PT_SHORT;
        return st;
    }
    if (t.isFloat) {
        if (isUnsigned != 0) return NULL;
        if (isSigned != 0) return NULL;

        if (cntLong == 0) {
            *to = PT_FLOAT;
            return st;
        }
        if (cntLong >= 1) {
            *to = PT_DOUBLE;
            return st;
        }

        error("Error cannot be there");
        return NULL;
    }
    if (t.isDouble) {
        if (isUnsigned != 0) return NULL;
        if (isSigned != 0) return NULL;

        *to = PT_DOUBLE;
        return st;
    }

    if (cntLong == 1) {
        if (isUnsigned) {
            *to = PT_ULONG;
            return st;
        }
        *to = PT_LONG;
        return st;
    }
    if (cntLong >= 2) {
        if (isUnsigned) {
            *to = PT_ULONGLONG;
            return st;
        }
        *to = PT_LONGLONG;
        return st;
    }

    *to = PT_INT;
    return st;
}

char* parseType(Token* tk, Type* to, char* st) {
    Type t;
    
    st = parseDeclarationType(tk, &t.pt, st);
    if (st == NULL) return NULL;

    t.pLevel = 0;

    while (match(TK_STAR)) t.pLevel++;
    *to = t;

    return st;
}

// parse: ( ^ expr...)
// Ret: SUCCESS, ERROR, MALLOC_ERROR
int parseBracketsExpression(Token* tk, Expression** toE, char* st, char** toS) {
    Expression* expr;
    int err = parseExpression(tk, &expr, st, &st);
    if (err != SUCCESS) return err;

    if (!match(TK_RPAREN)) {
        errExp(TK_RPAREN);
        freeExpression(expr);
        return ERROR;
    }

    *toE = expr;
    *toS = st;
    return SUCCESS;
}

// Ret: SUCCESS, ERROR, MALLOC_ERROR
// parse: ValueExpression & VariableExpression
int parseSimpleExpression(Token* tk, Expression** toE, char* st, char** toS) {
    Expression* expr;
    int err;

    if (is(TK_ID)) {
        expr = allocExpression(EXPR_VARIABLE);
        if (expr == NULL) {
            error("Memory allocation error");
            return MALLOC_ERROR;
        }
        strcpy(expr->ve.name, tk->id);
        next();
    }
    else if (is(TK_INT_CONSTANT)) {
        expr = allocExpression(EXPR_VALUE);
        if (expr == NULL) {
            error("Memory allocation error");
            return MALLOC_ERROR;
        }
        expr->vle.type.pLevel = 0;
        expr->vle.type.pt = PT_LONGLONG;
        expr->vle.ll = tk->iVal;
        next();
    }
    else if (is(TK_FLOAT_CONSTANT)) {
        expr = allocExpression(EXPR_VALUE);
        if (expr == NULL) {
            error("Memory allocation error");
            return MALLOC_ERROR;
        }
        expr->vle.type.pLevel = 0;
        expr->vle.type.pt = PT_DOUBLE;
        expr->vle.d = tk->fVal;
        next();
    }
    else {
        return ERROR;
    }
    

    *toE = expr;
    *toS = st;
    return SUCCESS;
}

// Ret: SUCCESS, ERROR, MALLOC_ERROR
int parseUnaryExpression(Token* tk, Expression** toE, char* st, char** toS, 
                        StackMathToken* prefixStack, StackMathToken* postfixStack) {
    Expression* expr = NULL;
    MathToken mt;
    int err;

    #define EXIT_ERROR { if (expr != NULL) freeExpression(expr); return err; }

    for (;;) {
        if (match(TK_LPAREN)) {
            if (isTypeBeginning(tk)) {
                mt.op = OPP_CAST;
                st = parseType(tk, &mt.castType, st);
                if (st == NULL) {
                    err = ERROR;
                    parserError("Cannot parse cast type");
                    EXIT_ERROR;
                }

                err = stackPushMathToken(prefixStack, &mt);
                if (err != SUCCESS) {
                    error("Memory allocation error");
                    EXIT_ERROR;
                }

                if (!match(TK_RPAREN)) {
                    err = ERROR;
                    errExp(TK_RPAREN);
                    EXIT_ERROR;
                }
            }
            else {
                err = parseBracketsExpression(tk, &expr, st, &st);
                if (err != SUCCESS) EXIT_ERROR;

                break;
            }
            continue;
        }
        if (isPrefixUnaryToken(tk)) {
            mt.op = tokenToPrefixOperator(tk);
            if (mt.op == -1) {
                err = ERROR;
                parserError("Cannot get prefix operator");
                EXIT_ERROR;
            }
            // not need to set priority
            err = stackPushMathToken(prefixStack, &mt);
            if (err != SUCCESS) {
                parserError("Cannot push unary token");
                EXIT_ERROR;
            }
            next();
            continue;
        }
        break;
    }

    if (expr == NULL) {
        err = parseSimpleExpression(tk, &expr, st, &st);
        if (err != SUCCESS) EXIT_ERROR;
    }

    if (match(TK_LPAREN_SQ)) {
        Expression* binary,* offset;
        binary = allocExpression(EXPR_BINARY);
        if (binary == NULL) {
            error("Memory allocation error");
            err = MALLOC_ERROR;
            EXIT_ERROR;
        }

        err = parseExpression(tk, &offset, st, &st);
        if (err != SUCCESS) {
            free(binary);
            EXIT_ERROR;
        }

        binary->be.op = OPB_SQ_BRACKETS;
        binary->be.expr1 = expr;
        binary->be.expr2 = offset;

        expr = binary;

        if (!match(TK_RPAREN_SQ)) {
            errExp(TK_RPAREN_SQ);
            err = ERROR;
            EXIT_ERROR;
        }
    }

    while (isPostfixUnaryToken(tk)) {
        mt.op = tokenToPostfixOperator(tk);
        if (mt.op == -1) {
            err = ERROR;
            parserError("Cannot get postfix operator");
            EXIT_ERROR;
        }
        // not need to set priority
        err = stackPushMathToken(postfixStack, &mt);
        if (err != SUCCESS) {
            parserError("Cannot push unary token");
            EXIT_ERROR;
        }
        next();
    }

    while (stackPopMathToken(postfixStack, &mt) == SUCCESS) {
        Expression* newExpr = allocExpression(EXPR_UNARY);
        if (newExpr == NULL) {
            err = MALLOC_ERROR;
            error("Memory allocation error");
            EXIT_ERROR;
        }
        newExpr->ue.op = (UnaryOperatorType) mt.op;
        newExpr->ue.expr = expr;
        expr = newExpr;
    }

    while (stackPopMathToken(prefixStack, &mt) == SUCCESS) {
        Expression* newExpr = allocExpression(mt.op != OPP_CAST ? EXPR_UNARY : EXPR_CAST);
        if (newExpr == NULL) {
            err = MALLOC_ERROR;
            error("Memory allocation error");
            EXIT_ERROR;
        }
        if (mt.op != OPP_CAST) {
            newExpr->ue.op = (UnaryOperatorType) mt.op;
            newExpr->ue.expr = expr;
        }
        else {
            newExpr->ce.expr = expr;
            newExpr->ce.type = mt.castType;
        }
        expr = newExpr;
    }

    *toE = expr;
    *toS = st;
    return SUCCESS;

    #undef EXIT_ERROR
}

// Ret: SUCCESS, ERROR, MALLOC_ERROR
int parseExpressionWithoutComma(Token* tk, Expression** toE, char* st, char** toS) {

    #define EXIT_ERROR { freeStackExpression(&exprStack); freeStackMathToken(&opStack); \
                         freeStackMathToken(&unaryStack1); freeStackMathToken(&unaryStack2); return ERROR; }
    StackExpression exprStack;
    StackMathToken opStack, unaryStack1, unaryStack2;

    MathToken mt;
    int err;

    err = stackInitMathToken(&opStack, OPERATOR_STACK_START_CAP);
    if (err != SUCCESS) {
        error("Memory allocation error");
        return err;
    }
    err = stackInitMathToken(&unaryStack1, OPERATOR_STACK_START_CAP);
    if (err != SUCCESS) {
        error("Memory allocation error");
        freeStackMathToken(&opStack);
        return err;
    }
    err = stackInitMathToken(&unaryStack2, OPERATOR_STACK_START_CAP);
    if (err != SUCCESS) {
        error("Memory allocation error");
        freeStackMathToken(&opStack);
        freeStackMathToken(&unaryStack1);
        return err;
    }
    err = stackInitExpression(&exprStack, EXPRESSION_STACK_START_CAP);
    if (err != SUCCESS) {
        error("Memory allocation error");
        freeStackMathToken(&opStack);
        freeStackMathToken(&unaryStack1);
        freeStackMathToken(&unaryStack2);
        return err;
    }

    for (; !is(TK_END) && !is(TK_RPAREN) && !is(TK_COMMA); ) {
        Expression* expr;
        MathToken t1, t2;

        err = parseUnaryExpression(tk, &expr, st, &st, &unaryStack1, &unaryStack2);
        if (err != SUCCESS) {
            parserError("Cannot parse unary expression");
            EXIT_ERROR;
        }
        err = stackPushExpression(&exprStack, &expr);
        if (err != SUCCESS) {
            parserError("Cannot push to stack");
            EXIT_ERROR;
        }

        if (is(TK_END) || is(TK_RPAREN) || is(TK_COMMA) || is(TK_RPAREN_SQ) || is(TK_RBR)) break;
        
        t1.op = tokenToBinaryOrAssignmentOperator(tk);
        if (t1.op == -1) {
            parserError("Bad binary operator `%s`", tk->id);
            EXIT_ERROR;
        }
        t1.pr = getOpPriority(t1.op);
        next();


        while (opStack.size != 0) {
            stackTopMathToken(&opStack, &t2);

            if (
                (t1.pr.isLeftAssoc && t1.pr.priority >= t2.pr.priority) ||
                (!t1.pr.isLeftAssoc && t1.pr.priority > t2.pr.priority)
            ) {
                ExpressionType te;
                Expression* expr;

                if (isBinary(t2.op)) {
                    expr = allocExpression(EXPR_BINARY);
                    if (expr == NULL) {
                        error("Memory allocation error");
                        EXIT_ERROR;
                    }
                    if ((err = stackPopExpression(&exprStack, &expr->be.expr2)) != SUCCESS) {
                        parserError("No expression to pop");
                        free(expr);
                        EXIT_ERROR;
                    }
                    if ((err = stackPopExpression(&exprStack, &expr->be.expr1)) != SUCCESS) {
                        parserError("No expression to pop");
                        freeExpression(expr->be.expr2);
                        free(expr);
                        EXIT_ERROR;
                    }
                    expr->be.op = (BinaryOperatorType) t2.op;
                }
                else if (isAssignment(t2.op)) {
                    expr = allocExpression(EXPR_ASSIGNMENT);
                    if (expr == NULL) {
                        error("Memory allocation error");
                        EXIT_ERROR;
                    }
                    if ((err = stackPopExpression(&exprStack, &expr->ae.expr2)) != SUCCESS) {
                        parserError("No expression to pop");
                        free(expr);
                        EXIT_ERROR;
                    }
                    if ((err = stackPopExpression(&exprStack, &expr->ae.expr1)) != SUCCESS) {
                        parserError("No expression to pop");
                        freeExpression(expr->be.expr2);
                        free(expr);
                        EXIT_ERROR;
                    }
                    expr->ae.op = (AssignmentOperatorType) t2.op;
                }
                else {
                    error("Bad token type %d", t2.op);
                    EXIT_ERROR;
                }

                stackPopMathToken(&opStack, NULL);
                
                if ((err = stackPushExpression(&exprStack, &expr)) != SUCCESS) {
                    error("Memory allocation error");
                    freeExpression(expr);
                    EXIT_ERROR;
                }
            }
            else break;
        }
        stackPushMathToken(&opStack, &t1);
    }

    while (stackPopMathToken(&opStack, &mt) == SUCCESS) {
        Expression* newExpr = allocExpression(isBinary(mt.op) ? EXPR_BINARY : EXPR_ASSIGNMENT);
        if (newExpr == NULL) {
            error("Memory allocation error");
            return MALLOC_ERROR;
        }

        if (isBinary(mt.op)) {
            if ((err = stackPopExpression(&exprStack, &newExpr->be.expr2)) != SUCCESS) {
                parserError("No expression to pop");
                free(newExpr);
                EXIT_ERROR;
            }
            if ((err = stackPopExpression(&exprStack, &newExpr->be.expr1)) != SUCCESS) {
                parserError("No expression to pop");
                freeExpression(newExpr->be.expr2);
                free(newExpr);
                EXIT_ERROR;
            }
            newExpr->be.op = (BinaryOperatorType) mt.op;
        }
        else {
            if ((err = stackPopExpression(&exprStack, &newExpr->ae.expr2)) != SUCCESS) {
                parserError("No expression to pop");
                free(newExpr);
                EXIT_ERROR;
            }
            if ((err = stackPopExpression(&exprStack, &newExpr->ae.expr1)) != SUCCESS) {
                parserError("No expression to pop");
                freeExpression(newExpr->be.expr2);
                free(newExpr);
                EXIT_ERROR;
            }
            newExpr->ae.op = (AssignmentOperatorType) mt.op;
        }
        
        if ((err = stackPushExpression(&exprStack, &newExpr)) != SUCCESS) {
            error("Memory allocation error");
            EXIT_ERROR;
        }
    }

    if ((err = stackPopExpression(&exprStack, toE)) != SUCCESS) {
        parserError("No expression to pop");
        EXIT_ERROR;
    }

    *toS = st;
    freeStackExpression(&exprStack);
    freeStackMathToken(&opStack);
    freeStackMathToken(&unaryStack1);
    freeStackMathToken(&unaryStack2);
    return SUCCESS;

    #undef EXIT_ERROR
}

// Ret: SUCCESS, ERROR, MALLOC_ERROR
int parseExpression(Token* tk, Expression** toE, char* st, char** toS) {
    Expression* expr;
    Expression* commaExpr;
    ExpressionList* exprList;
    ExpressionList* last;
    int err;

    if ((err = parseExpressionWithoutComma(tk, &expr, st, &st)) != SUCCESS) return err;

    if (!is(TK_COMMA)) {
        *toE = expr;
        *toS = st;
        return SUCCESS;
    }

    commaExpr = (Expression*) malloc(sizeof(*commaExpr));
    if (commaExpr == NULL) {
        error("Cannot allocate memory");
        freeExpression(expr);
        return MALLOC_ERROR;
    }
    commaExpr->type = EXPR_COMMA;

    exprList = (ExpressionList*) malloc(sizeof(*exprList));
    if (exprList == NULL) {
        error("Cannot allocate memory");
        freeExpression(expr);
        free(commaExpr);
        return MALLOC_ERROR;
    }
    exprList->expr = expr;
    exprList->next = NULL;
    last = exprList;

    commaExpr->cme.exprs = exprList;

    while (match(TK_COMMA)) {
        ExpressionList* newNode;

        err = parseExpressionWithoutComma(tk, &expr, st, &st);
        if (err != SUCCESS) {
            freeExpression(commaExpr); // list must be freed automatically
            return err;
        }

        newNode = (ExpressionList*) malloc(sizeof(*newNode));
        if (newNode == NULL) {
            error("Cannot allocate memory");
            freeExpression(commaExpr);
            return MALLOC_ERROR;
        }
        newNode->expr = expr;
        newNode->next = NULL;
        last->next = newNode;
        last = newNode;
    }

    *toE = commaExpr;
    *toS = st;
    return SUCCESS;
}

// Ret: SUCCESS, ERROR, MALLOC_ERROR
// adds new statement to ->next
int parseVarDeclarationStatement(Token* tk, StatementList* last, char* st) {
    StatementList* node;
    PrimitiveType pt;
    int* vAmount, isUnsigned = 0;

    st = parseDeclarationType(tk, &pt, st);
    if (st == NULL) {
        parserError("Cannot parse type of variable declaration");
        return ERROR;
    }

    node = allocStatementList();

    if (node == NULL) return MALLOC_ERROR;
    vAmount = &node->st.vs.vAmount;

    node->st.type = ST_VARIABLE_DECLARATION;
    node->st.vs.vType = pt;

    for (; *vAmount < MAX_VARS_PER_STATEMENT;) {
        VarDeclField* fld = &node->st.vs.variables[*vAmount];
        
        while (match(TK_STAR)) fld->pLevel++;

        if (!is(TK_ID)) {
            errExp(TK_ID);
            freeStatementList(node);
            node = NULL;

            return ERROR;
        }

        strcpy(fld->name, tk->id);
        next();
        
        if (match(TK_LPAREN_SQ)) {
            fld->isArray = 1;
            if (match(TK_RPAREN_SQ));
            else if (is(TK_INT_CONSTANT)) {
                fld->arraySize = tk->iVal;
                next();
                if (!match(TK_RPAREN_SQ)) {
                    errExp(TK_RPAREN_SQ);
                    freeStatementList(node);
                    return ERROR;
                }
            }
            else {
                parserError("Cannot parse [] after variable name");
                freeStatementList(node);
                return ERROR;
            }
        }

        if (match(TK_EQ)) {
            if (!fld->isArray) {
                int err = parseExpressionWithoutComma(tk, &fld->expr, st, &st);
                if (err != SUCCESS) {
                    parserError("Cannot parse expression in variable declaration");
                    freeStatementList(node);
                    return err;
                }
            }
            else {
                ExpressionList list;
                ExpressionList* last = &list;
                int err;

                list.next = NULL;
                fld->arrDataPtr = NULL;

                if (!match(TK_LBR)) {
                    errExp(TK_LBR);
                    freeStatementList(node);
                    return ERROR;
                }

                for (;;) {
                    ExpressionList* newNode;

                    if (is(TK_RBR)) break;

                    newNode = (ExpressionList*) malloc(sizeof(*newNode));
                    if (newNode == NULL) {
                        error("Memory allocation error");
                        freeStatementList(node);
                        freeExpressionList(list.next);
                        return MALLOC_ERROR;
                    }
                    newNode->next = NULL;
                    
                    err = parseExpressionWithoutComma(tk, &newNode->expr, st, &st);
                    if (err != SUCCESS) {
                        freeStatementList(node);
                        freeExpressionList(list.next);
                        return err;
                    }

                    last->next = newNode;
                    last = newNode;

                    if (!match(TK_COMMA)) break;
                }

                fld->exprList = list.next;

                if (!match(TK_RBR)) {
                    errExp(TK_RBR);
                    freeStatementList(node);
                    freeExpressionList(list.next);
                    return ERROR;
                }
            }
        }
        if (match(TK_COMMA)) {
            (*vAmount)++;
            continue;
        }
        if (is(TK_END)) {
            break;
        }
    }

    (*vAmount)++;
    last->next = node;
    return SUCCESS;
}

int parseExpressionStatement(Token* tk, StatementList* last, char* st) {
    StatementList* node;
    Expression* expr;
    int err = parseExpression(tk, &expr, st, &st);

    if (err != SUCCESS) return err;

    node = allocStatementList();
    if (node == NULL) {
        freeExpression(expr);
        expr = NULL;
        return MALLOC_ERROR;
    }

    node->st.type = ST_EXPRESSION;
    node->st.es.expr = expr;

    last->next = node;
    return SUCCESS;
}

// Ret: SUCCESS, ERROR, MALLOC_ERROR
// print ^ expr...;
int parsePrintStatement(Token* tk, StatementList* last, char* st) {
    StatementList* newL = allocStatementList();
    if (newL == NULL) {
        error("Memory allocation error");
        return MALLOC_ERROR;
    }
    newL->st.type = ST_PRINT;
    if (parseExpression(tk, &newL->st.ps.expr, st, &st) != SUCCESS) {
        parserError("Cannot parse expression after print");
        free(newL);
        return ERROR;
    }
    last->next = newL;
    return SUCCESS;
}

// Ret: SUCCESS, ERROR, MALLOC_ERROR
// adds new statement to ->next
int parseStatement(StatementList* last, char* st) {
    Token token;
    Token* tk = &token;

    next();

    if (isTypeBeginning(tk)) {
        return parseVarDeclarationStatement(tk, last, st);
    }
    else if (match(TK_PRINT)) {
        return parsePrintStatement(tk, last, st);
    }

    return parseExpressionStatement(tk, last, st);
}

#undef is
#undef match
#undef next
#undef parserError
#undef errExp

// Ret: SUCCESS, ERROR, MALLOC_ERROR
int parse(StatementList** listOut, FILE* from) {
    StatementList base;
    StatementList* last = &base;
    base.next = NULL;

    for (;;) {
        char statement[PARSER_LINE_BUFFER], *s;
        int err;

        s = fgetsd(statement, PARSER_LINE_BUFFER, from, ';');
        if (s == NULL) break;
        s = strskp(s);
        if (!*s) break;

        err = parseStatement(last, s);
        if (err != SUCCESS) {
            freeStatementList(base.next);
            base.next = NULL;
            return err;
        }
        
        if (last->next != NULL) {
            char* copyWhat = strskp(statement);
            size_t l = strlen(copyWhat);
            char* codeLine = (char*) malloc((l + 1) * sizeof(*codeLine));

            if (codeLine == NULL) {
                error("Memory allocation error");
                freeStatementList(base.next);
                return MALLOC_ERROR;
            }

            strcpy(codeLine, copyWhat);

            last = last->next;
            last->st.codeLine = codeLine;
        }
    }

    *listOut = base.next;
    return SUCCESS;
}

int main() {
    Context ctx;
    StatementList* l = NULL,* node;
    int lineCounter;

    ctx.varList = NULL;
    ctx.memRegList = NULL;
    ctx.hasEvaluationError = 0;

    printf("Enter linear C code:\n\n");

    int err = parse(&l, stdin);
    if (err == ERROR) {
        printf("\n====== ERROR ======\n");
        printf("Ends with parsing error\n");
        freeCtxVarList(ctx.varList);
        freeCtxMemRegList(ctx.memRegList);
        freeStatementList(l);
        return 1;
    }
    else if (err == MALLOC_ERROR) {
        printf("\n====== ERROR ======\n");
        printf("Ends with malloc error\n");
        freeCtxVarList(ctx.varList);
        freeCtxMemRegList(ctx.memRegList);
        freeStatementList(l);
        return 2;
    }

    printf("\n======= OUT =======\n\n");
    for (node = l, lineCounter = 1; node; node = node->next, lineCounter++) {
        int fChg = 0;
        int err = interpretStatement(&fChg, &ctx, &node->st);

        if (err == MALLOC_ERROR) {
            printf("Ends with malloc error\n");
            freeCtxVarList(ctx.varList);
            freeCtxMemRegList(ctx.memRegList);
            freeStatementList(l);
            return 2;
        }
        
        if (ctx.hasEvaluationError) {
            printf("Error occurred in the line %d:\n", lineCounter);
            printf("%s;\n", node->st.codeLine);
            break;
        }

        if (fChg) {
            printf("%s;\n", node->st.codeLine);
        }
    }

    if (!ctx.hasEvaluationError) {
        printf("\n===== SUCCESS =====\n");
    }

    freeCtxVarList(ctx.varList);
    freeCtxMemRegList(ctx.memRegList);
    freeStatementList(l);
    return 0;
}
