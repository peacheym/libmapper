#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapper_internal.h"

#define STACK_SIZE 128
#ifdef DEBUG
#define TRACING 0 /* Set non-zero to see trace during parse & eval. */
#else
#define TRACING 0
#endif

static int mini(int x, int y)
{
    if (y < x) return y;
    else return x;
}

static float minf(float x, float y)
{
    if (y < x) return y;
    else return x;
}

static double mind(double x, double y)
{
    if (y < x) return y;
    else return x;
}

static int maxi(int x, int y)
{
    if (y > x) return y;
    else return x;
}

static float maxf(float x, float y)
{
    if (y > x) return y;
    else return x;
}

static double maxd(double x, double y)
{
    if (y > x) return y;
    else return x;
}

static float pif()
{
    return M_PI;
}

static double pid()
{
    return M_PI;
}

static float ef()
{
    return M_E;
}

static double ed()
{
    return M_E;
}

static float midiToHzf(float x)
{
    return 440. * pow(2.0, (x - 69) / 12.0);
}

static double midiToHzd(double x)
{
    return 440. * pow(2.0, (x - 69) / 12.0);
}

static float hzToMidif(float x)
{
    return 69. + 12. * log2(x / 440.);
}

static double hzToMidid(double x)
{
    return 69. + 12. * log2(x / 440.);
}

static float uniformf(float x)
{
    return rand() / (RAND_MAX + 1.0) * x;
}

static double uniformd(double x)
{
    return rand() / (RAND_MAX + 1.0) * x;
}

typedef enum {
    VAR_UNKNOWN=-1,
    VAR_X=0,
    VAR_Y,
    N_VARS
} expr_var_t;

const char *var_strings[] =
{
    "x",
    "y",
};

typedef enum {
    FUNC_UNKNOWN=-1,
    FUNC_ABS=0,
    FUNC_ACOS,
    FUNC_ACOSH,
    FUNC_ASIN,
    FUNC_ASINH,
    FUNC_ATAN,
    FUNC_ATAN2,
    FUNC_ATANH,
    FUNC_CBRT,
    FUNC_CEIL,
    FUNC_COS,
    FUNC_COSH,
    FUNC_E,
    FUNC_EXP,
    FUNC_EXP2,
    FUNC_FLOOR,
    FUNC_HYPOT,
    FUNC_HZTOMIDI,
    FUNC_LOG,
    FUNC_LOG10,
    FUNC_LOG2,
    FUNC_LOGB,
    FUNC_MAX,
    FUNC_MIDITOHZ,
    FUNC_MIN,
    FUNC_PI,
    FUNC_POW,
    FUNC_ROUND,
    FUNC_SIN,
    FUNC_SINH,
    FUNC_SQRT,
    FUNC_TAN,
    FUNC_TANH,
    FUNC_TRUNC,
    FUNC_UNIFORM,
    N_FUNCS
} expr_func_t;

static struct {
    const char *name;
    unsigned int arity;
    void *func_int32;
    void *func_float;
    void *func_double;
} function_table[] = {
    { "abs",      1,    abs,        fabsf,      fabs        },
    { "acos",     1,    0,          acosf,      acos        },
    { "acosh",    1,    0,          acoshf,     acosh       },
    { "asin",     1,    0,          asinf,      asin        },
    { "asinh",    1,    0,          asinhf,     asinh       },
    { "atan",     1,    0,          atanf,      atan        },
    { "atan2",    2,    0,          atan2f,     atan2       },
    { "atanh",    1,    0,          atanhf,     atanh       },
    { "cbrt",     1,    0,          cbrtf,      cbrt        },
    { "ceil",     1,    0,          ceilf,      ceil        },
    { "cos",      1,    0,          cosf,       cos         },
    { "cosh",     1,    0,          coshf,      cosh        },
    { "e",        0,    0,          ef,         ed          },
    { "exp",      1,    0,          expf,       exp         },
    { "exp2",     1,    0,          exp2f,      exp2        },
    { "floor",    1,    0,          floorf,     floor       },
    { "hypot",    2,    0,          hypotf,     hypot       },
    { "hzToMidi", 1,    0,          hzToMidif,  hzToMidid   },
    { "log",      1,    0,          logf,       log         },
    { "log10",    1,    0,          log10f,     log10       },
    { "log2",     1,    0,          log2f,      log2        },
    { "logb",     1,    0,          logbf,      logb        },
    { "max",      2,    maxi,       maxf,       maxd        },
    { "midiToHz", 1,    0,          midiToHzf,  midiToHzd   },
    { "min",      2,    mini,       minf,       mind        },
    { "pi",       0,    0,          pif,        pid         },
    { "pow",      2,    0,          powf,       pow         },
    { "round",    1,    0,          roundf,     round       },
    { "sin",      1,    0,          sinf,       sin         },
    { "sinh",     1,    0,          sinhf,      sinh        },
    { "sqrt",     1,    0,          sqrtf,      sqrt        },
    { "tan",      1,    0,          tanf,       tan         },
    { "tanh",     1,    0,          tanhf,      tanh        },
    { "trunc",    1,    0,          truncf,     trunc       },
    { "uniform",  1,    0,          uniformf,   uniformd    },
};

typedef enum {
    OP_LOGICAL_NOT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_ADD,
    OP_SUBTRACT,
    OP_LEFT_BIT_SHIFT,
    OP_RIGHT_BIT_SHIFT,
    OP_IS_GREATER_THAN,
    OP_IS_GREATER_THAN_OR_EQUAL,
    OP_IS_LESS_THAN,
    OP_IS_LESS_THAN_OR_EQUAL,
    OP_IS_EQUAL,
    OP_IS_NOT_EQUAL,
    OP_BITWISE_AND,
    OP_BITWISE_XOR,
    OP_BITWISE_OR,
    OP_LOGICAL_AND,
    OP_LOGICAL_OR,
    OP_ASSIGNMENT,
    OP_CONDITIONAL_IF,
    OP_CONDITIONAL_IF_ELSE,
} expr_op_t;

static struct {
    const char *name;
    unsigned int arity;
    unsigned int precedence;
} op_table[] = {
    { "!",      1,  11 },
    { "*",      2,  10 },
    { "/",      2,  10 },
    { "%",      2,  10 },
    { "+",      2,   9 },
    { "-",      2,   9 },
    { "<<",     2,   8 },
    { ">>",     2,   8 },
    { ">",      2,   7 },
    { ">=",     2,   7 },
    { "<",      2,   7 },
    { "<=",     2,   7 },
    { "==",     2,   6 },
    { "!=",     2,   6 },
    { "&",      2,   5 },
    { "^",      2,   4 },
    { "|",      2,   3 },
    { "&&",     2,   2 },
    { "||",     2,   1 },
    { "=",      2,   0 },
    { "IF",     2,   0 },
    { "IFELSE", 3,   0 },
};

typedef int func_int32_arity0();
typedef int func_int32_arity1(int);
typedef int func_int32_arity2(int,int);
typedef float func_float_arity0();
typedef float func_float_arity1(float);
typedef float func_float_arity2(float,float);
typedef double func_double_arity0();
typedef double func_double_arity1(double);
typedef double func_double_arity2(double,double);

typedef struct _token {
    enum {
        TOK_CONST,
        TOK_OP,
        TOK_VAR,
        TOK_FUNC,
        TOK_OPEN_PAREN,
        TOK_CLOSE_PAREN,
        TOK_OPEN_SQUARE,
        TOK_CLOSE_SQUARE,
        TOK_OPEN_CURLY,
        TOK_CLOSE_CURLY,
        TOK_COMMA,
        TOK_QUESTION,
        TOK_COLON,
        TOK_NEGATE,
        TOK_VECTORIZE,
        TOK_END,
    } toktype;
    union {
        float f;
        int i;
        double d;
        expr_var_t var;
        expr_op_t op;
        expr_func_t func;
    };
    char datatype;
    char casttype;
    char history_index;
    char vector_length_locked;
    int vector_length;
    int vector_index;
} mapper_token_t, *mapper_token;

static expr_func_t function_lookup(const char *s, int len)
{
    int i;
    for (i=0; i<N_FUNCS; i++) {
        if (strncmp(s, function_table[i].name, len)==0)
            return i;
    }
    return FUNC_UNKNOWN;
}

static expr_var_t variable_lookup(const char *s, int len)
{
    int i;
    for (i=0; i<N_VARS; i++) {
        if (strncmp(s, var_strings[i], len)==0)
            return i;
    }
    return VAR_UNKNOWN;
}

static int expr_lex(const char *str, int index, mapper_token_t *tok)
{
    tok->datatype = 'i';
    tok->casttype = 0;
    tok->vector_length = 1;
    tok->vector_index = 0;
    tok->vector_length_locked = 0;
    int n, i;
    char c = str[index];
    int integer_found = 0;

    if (c==0) {
        tok->toktype = TOK_END;
        return index;
    }

  again:

    i = index;
    if (isdigit(c)) {
        do {
            c = str[++index];
        } while (c && isdigit(c));
        n = atoi(str+i);
        integer_found = 1;
        if (c!='.' && c!='e') {
            tok->i = n;
            tok->toktype = TOK_CONST;
            tok->datatype = 'i';
            return index;
        }
    }

    switch (c) {
    case '.':
        c = str[++index];
        if (!isdigit(c) && c!='e' && integer_found) {
            tok->toktype = TOK_CONST;
            tok->f = (float)n;
            tok->datatype = 'f';
            return index;
        }
        if (!isdigit(c) && c!='e')
            break;
        do {
            c = str[++index];
        } while (c && isdigit(c));
        if (c!='e') {
            tok->f = atof(str+i);
            tok->toktype = TOK_CONST;
            tok->datatype = 'f';
            return index;
        }
    case 'e':
        if (!integer_found) {
            n = index;
            while (c && (isalpha(c) || isdigit(c)))
                c = str[++index];
            tok->toktype = TOK_FUNC;
            tok->func = function_lookup(str+i, index-i);
            if (tok->func == FUNC_UNKNOWN) {
                printf("unexpected `e' outside float\n");
                break;
            }
            else
                return index;
        }
        c = str[++index];
        if (c!='-' && c!='+' && !isdigit(c)) {
            printf("Incomplete scientific notation `%s'.\n",str+i);
            break;
        }
        if (c=='-' || c=='+')
            c = str[++index];
        while (c && isdigit(c))
            c = str[++index];
        tok->toktype = TOK_CONST;
        tok->datatype = 'f';
        tok->f = atof(str+i);
        return index;
    case '+':
        tok->toktype = TOK_OP;
        tok->op = OP_ADD;
        return ++index;
    case '-':
        // could be either subtraction or negation
        i = index-1;
        // back up one character
        while (i && strchr(" \t\r\n", str[i]))
           i--;
        if (isalpha(str[i]) || isdigit(str[i]) || strchr(")]}", str[i])) {
            tok->toktype = TOK_OP;
            tok->op = OP_SUBTRACT;
        }
        else {
            tok->toktype = TOK_NEGATE;
        }
        return ++index;
    case '/':
        tok->toktype = TOK_OP;
        tok->op = OP_DIVIDE;
        return ++index;
    case '*':
        tok->toktype = TOK_OP;
        tok->op = OP_MULTIPLY;
        return ++index;
    case '%':
        tok->toktype = TOK_OP;
        tok->op = OP_MODULO;
        return ++index;
    case '=':
        // could be '=', '=='
        tok->toktype = TOK_OP;
        tok->op = OP_ASSIGNMENT;
        c = str[++index];
        if (c == '=') {
            tok->op = OP_IS_EQUAL;
            ++index;
        }
        return index;
    case '<':
        // could be '<', '<=', '<<'
        tok->toktype = TOK_OP;
        tok->op = OP_IS_LESS_THAN;
        c = str[++index];
        if (c == '=') {
            tok->op = OP_IS_LESS_THAN_OR_EQUAL;
            ++index;
        }
        else if (c == '<') {
            tok->op = OP_LEFT_BIT_SHIFT;
            ++index;
        }
        return index;
    case '>':
        // could be '>', '>=', '>>'
        tok->toktype = TOK_OP;
        tok->op = OP_IS_GREATER_THAN;
        c = str[++index];
        if (c == '=') {
            tok->op = OP_IS_GREATER_THAN_OR_EQUAL;
            ++index;
        }
        else if (c == '>') {
            tok->op = OP_RIGHT_BIT_SHIFT;
            ++index;
        }
        return index;
    case '!':
        // could be '!', '!='
        // TODO: handle factorial and logical negation cases
        tok->toktype = TOK_OP;
        tok->op = OP_LOGICAL_NOT;
        c = str[++index];
        if (c == '=') {
            tok->op = OP_IS_NOT_EQUAL;
            ++index;
        }
        return index;
    case '&':
        // could be '&', '&&'
        tok->toktype = TOK_OP;
        tok->op = OP_BITWISE_AND;
        c = str[++index];
        if (c == '&') {
            tok->op = OP_LOGICAL_AND;
            ++index;
        }
        return index;
    case '|':
        // could be '|', '||'
        tok->toktype = TOK_OP;
        tok->op = OP_BITWISE_OR;
        c = str[++index];
        if (c == '|') {
            tok->op = OP_LOGICAL_OR;
            ++index;
        }
        return index;
    case '^':
        // bitwise XOR
        tok->toktype = TOK_OP;
        tok->op = OP_BITWISE_XOR;
        return ++index;
    case '(':
        tok->toktype = TOK_OPEN_PAREN;
        return ++index;
    case ')':
        tok->toktype = TOK_CLOSE_PAREN;
        return ++index;
    case '[':
        tok->toktype = TOK_OPEN_SQUARE;
        return ++index;
    case ']':
        tok->toktype = TOK_CLOSE_SQUARE;
        return ++index;
    case '{':
        tok->toktype = TOK_OPEN_CURLY;
        return ++index;
    case '}':
        tok->toktype = TOK_CLOSE_CURLY;
        return ++index;
    case ' ':
    case '\t':
    case '\r':
    case '\n':
        c = str[++index];
        goto again;
    case ',':
        tok->toktype = TOK_COMMA;
        return ++index;
    case '?':
        // conditional
        tok->toktype = TOK_QUESTION;
        return ++index;
    case ':':
        tok->toktype = TOK_COLON;
        return ++index;
    default:
        if (!isalpha(c)) {
            printf("unknown character '%c' in lexer\n", c);
            break;
        }
        while (c && (isalpha(c) || isdigit(c)))
            c = str[++index];
        if ((tok->var = variable_lookup(str+i, index-i)) != VAR_UNKNOWN) {
            tok->toktype = TOK_VAR;
        }
        else if ((tok->func = function_lookup(str+i, index-i)) != FUNC_UNKNOWN) {
            tok->toktype = TOK_FUNC;
        }
        else {
            printf("Unknown variable or function name `%s'.\n",str+i);
            break;
        }
        return index;
    }

    return 1;
}

struct _mapper_expr
{
    mapper_token start;
    int length;
    int vector_size;
    int input_history_size;
    int output_history_size;
};

typedef enum {
    YEQUAL_Y,
    YEQUAL_EQ,
    EXPR,
    EXPR_RIGHT,
    TERM,
    TERM_RIGHT,
    VALUE,
    NEGATE,
    NOT,
    VAR_RIGHT,
    VAR_VECTINDEX,
    VAR_HISTINDEX,
    CLOSE_VECTINDEX,
    CLOSE_HISTINDEX,
    OPEN_PAREN,
    CLOSE_PAREN,
    COMMA,
    QUESTION,
    COLON,
    END,
} state_t;

void mapper_expr_free(mapper_expr expr)
{
    free(expr);
}

#ifdef DEBUG

void printtoken(mapper_token_t tok)
{
    int locked = tok.vector_length_locked;
    switch (tok.toktype) {
    case TOK_CONST:
        switch (tok.datatype) {
            case 'f':      printf("%ff%d", tok.f,
                                  tok.vector_length);     break;
            case 'd':      printf("%fd%d", tok.d,
                                  tok.vector_length);     break;
            case 'i':      printf("%di%d", tok.i,
                                  tok.vector_length);     break;
        }
        if (locked)        printf("'");                   break;
    case TOK_OP:           printf("%s%c%d%s",
                                  op_table[tok.op].name,
                                  tok.datatype,
                                  tok.vector_length,
                                  locked ? "'" : "");     break;
    case TOK_OPEN_PAREN:   printf("(");                   break;
    case TOK_CLOSE_PAREN:  printf(")");                   break;
    case TOK_VAR:          printf("VAR(%s%c%d%s){%d}[%d]",
                                  var_strings[tok.var], tok.datatype,
                                  tok.vector_length,
                                  locked ? "'" : "",
                                  tok.history_index,
                                  tok.vector_index);      break;
    case TOK_FUNC:         printf("FUNC(%s)%c%d%s",
                                  function_table[tok.func].name,
                                  tok.datatype,
                                  tok.vector_length,
                                  locked ? "'" : "");     break;
    case TOK_COMMA:        printf(",");                   break;
    case TOK_QUESTION:     printf("?");                   break;
    case TOK_COLON:        printf(":");                   break;
    case TOK_VECTORIZE:    printf("VECT%c%d(%d)",
                                  tok.datatype,
                                  tok.vector_length,
                                  tok.vector_index);      break;
    case TOK_END:          printf("END");                 break;
    default:               printf("(unknown token)");     break;
    }
    if (tok.casttype)
        printf("->%c", tok.casttype);
}

void printstack(const char *s, mapper_token_t *stack, int top)
{
    int i;
    printf("%s ", s);
    for (i=0; i<=top; i++) {
        printtoken(stack[i]);
        printf(" ");
    }
    printf("\n");
}

void printexpr(const char *s, mapper_expr e)
{
    printstack(s, e->start, e->length-1);
}

#endif

static char compare_token_datatype(mapper_token_t tok, char type)
{
    // return the higher datatype
    if (tok.datatype == 'd' || tok.casttype == 'd' || type == 'd')
        return 'd';
    else if (tok.datatype == 'f' || tok.casttype == 'f' || type == 'f')
        return 'f';
    else
        return 'i';
}

static void promote_token_datatype(mapper_token_t *tok, char type)
{
    if (tok->datatype == type)
        return;

    if (tok->toktype == TOK_CONST) {
        // constants can be cast immediately
        if (tok->datatype == 'i') {
            if (type == 'f') {
                tok->f = (float)tok->i;
                tok->datatype = 'f';
            }
            else if (type == 'd') {
                tok->d = (double)tok->i;
                tok->datatype = 'd';
            }
        }
        else if (tok->datatype == 'f') {
            if (type == 'd') {
                tok->d = (double)tok->f;
                tok->datatype = 'd';
            }
        }
    }
    else if (tok->toktype == TOK_VAR) {
        // we need to cast at runtime
        if (tok->datatype == 'i' || type == 'd')
            tok->casttype = type;
    }
    else {
        if (tok->datatype == 'i' || type == 'd')
            tok->datatype = type;
    }
}

static void lock_vector_lengths(mapper_token_t *stack, int top)
{
    int i=top, arity=1;

    while ((i >= 0) && arity--) {
        stack[i].vector_length_locked = 1;
        if (stack[i].toktype == TOK_OP)
            arity += op_table[stack[i].op].arity;
        else if (stack[i].toktype == TOK_FUNC)
            arity += function_table[stack[i].func].arity;
        else if (stack[i].toktype == TOK_VECTORIZE)
            arity += stack[i].vector_index;
        i--;
    }
}

static int check_types_and_lengths(mapper_token_t *stack, int top)
{
    int i, arity, can_precompute = 1;
    char type = stack[top].datatype;
    int vector_length = stack[top].vector_length;

    if (stack[top].toktype == TOK_OP)
        arity = op_table[stack[top].op].arity;
    else if (stack[top].toktype == TOK_FUNC) {
        arity = function_table[stack[top].func].arity;
        if (stack[top].func == FUNC_UNIFORM)
            can_precompute = 0;
    }
    else if (stack[top].toktype == TOK_VECTORIZE) {
        arity = stack[top].vector_index;
        can_precompute = 0;
    }
    else
        return top;

    if (arity) {
        // find operator or function inputs
        i = top;
        int skip = 0;
        int depth = arity;
        // last arg of op or func is at top-1
        type = compare_token_datatype(stack[top-1], type);
        if (stack[top-1].vector_length > vector_length)
            vector_length = stack[top-1].vector_length;

        /* walk down stack distance of arity, checking datatypes
         * and vector lengths */
        while (--i >= 0) {
            if (stack[i].toktype == TOK_FUNC &&
                function_table[stack[i].func].arity)
                can_precompute = 0;
            else if (stack[i].toktype != TOK_CONST)
                can_precompute = 0;

            if (skip == 0) {
                type = compare_token_datatype(stack[i], type);
                if (stack[i].vector_length > vector_length)
                    vector_length = stack[i].vector_length;
                depth--;
                if (depth == 0)
                    break;
            }
            else
                skip--;
            if (stack[i].toktype == TOK_OP)
                skip += op_table[stack[i].op].arity;
            else if (stack[i].toktype == TOK_FUNC)
                skip += function_table[stack[i].func].arity;
            else if (stack[i].toktype == TOK_VECTORIZE)
                skip += stack[i].vector_index;
        }

        if (depth)
            return -1;

        /* walk down stack distance of arity again, promoting datatypes
         * and vector lengths */
        i = top;
        if (stack[top].toktype == TOK_VECTORIZE) {
            skip = stack[top].vector_index;
            depth = 0;
        }
        else {
            skip = 0;
            depth = arity;
        }
        while (--i >= 0) {
            // we will promote everything within range of compound arity
            promote_token_datatype(&stack[i], type);

            if (skip <= 0) {
                // also check/promote vector length
                if (!stack[i].vector_length_locked)
                    stack[i].vector_length = vector_length;
                else if (stack[i].vector_length != vector_length) {
                    printf("Vector length mismatch (%d != %d).\n",
                           stack[i].vector_length, vector_length);
                    return -1;
                }
            }

            if (stack[i].toktype == TOK_OP) {
                depth += op_table[stack[i].op].arity;
                if (skip > 0)
                    skip += op_table[stack[i].op].arity;
            }
            else if (stack[i].toktype == TOK_FUNC) {
                depth += function_table[stack[i].func].arity;
                if (skip > 0)
                    skip += function_table[stack[i].func].arity;
            }
            else if (stack[i].toktype == TOK_VECTORIZE)
                skip = stack[i].vector_index+1;

            if (skip > 0)
                skip--;
            else {
                depth--;
                if (depth <= 0)
                    break;
            }
        }
        stack[top].datatype = type;
        if (!stack[top].vector_length_locked)
            stack[top].vector_length = vector_length;
        else if (stack[top].vector_length != vector_length) {
            printf("Vector length mismatch (%d != %d).\n",
                   stack[i].vector_length, vector_length);
            return -1;
        }
    }
    else {
        stack[top].datatype = 'f';
    }

    // if stack within bounds of arity was only constants, we're ok to compute
    if (!can_precompute)
        return top;

    struct _mapper_expr e;
    e.start = &stack[top-arity];
    e.length = arity+1;
    mapper_signal_history_t h;
    mapper_signal_value_t v;
    h.type = stack[top].datatype;
    h.value = &v;
    h.position = -1;
    h.length = 1;
    h.size = 1;
    if (!mapper_expr_evaluate(&e, 0, &h))
        return top;

    switch (stack[top].datatype) {
        case 'f':
            stack[top-arity].f = v.f;
            break;
        case 'd':
            stack[top-arity].d = v.d;
            break;
        case 'i':
            stack[top-arity].i = v.i32;
            break;
        default:
            return 0;
            break;
    }
    stack[top-arity].toktype = TOK_CONST;
    return top-arity;
}

/* Macros to help express stack operations in parser. */
#define FAIL(msg) { printf("%s\n", msg); return 0; }
#define PUSH_TO_OUTPUT(x)                                           \
{                                                                   \
    if (++outstack_index >= STACK_SIZE)                             \
        {FAIL("Stack size exceeded.");}                             \
    memcpy(outstack + outstack_index, &x, sizeof(mapper_token_t));  \
}
#define PUSH_TO_OPERATOR(x)                                         \
{                                                                   \
    if (++opstack_index >= STACK_SIZE)                              \
        {FAIL("Stack size exceeded.");}                             \
    memcpy(opstack + opstack_index, &x, sizeof(mapper_token_t));    \
}   
#define POP_OPERATOR() ( opstack_index-- )
#define POP_OPERATOR_TO_OUTPUT()                                    \
{                                                                   \
    if (opstack[opstack_index].toktype == TOK_QUESTION) {           \
        opstack[opstack_index].toktype = TOK_OP;                    \
        opstack[opstack_index].op = OP_CONDITIONAL_IF;              \
    }                                                               \
    PUSH_TO_OUTPUT(opstack[opstack_index]);                         \
    outstack_index = check_types_and_lengths(outstack,              \
                                             outstack_index);       \
    if (outstack_index < 0)                                         \
         {FAIL("Malformed expression.");}                           \
    POP_OPERATOR();                                                 \
}
#define GET_NEXT_TOKEN(x)                                           \
{                                                                   \
    lex_index = expr_lex(str, lex_index, &x);                       \
    if (!lex_index)                                                 \
        {FAIL("Error in lexer.");}                                  \
}

/*! Use Dijkstra's shunting-yard algorithm to parse expression into RPN stack. */
mapper_expr mapper_expr_new_from_string(const char *str,
                                        char input_type,
                                        char output_type,
                                        int input_vector_size,
                                        int output_vector_size,
                                        int *input_history_size,
                                        int *output_history_size)
{
    if (!str) return 0;
    if (input_type != 'i' && input_type != 'f' && input_type != 'd') return 0;
    if (output_type != 'i' && output_type != 'f' && output_type != 'd') return 0;

    mapper_token_t outstack[STACK_SIZE];
    mapper_token_t opstack[STACK_SIZE];
    int lex_index = 0, outstack_index = -1, opstack_index = -1;
    int oldest_input = 0, oldest_output = 0, max_vector = 1;
    int vectorizing = 0;
    int variable = 0;
    mapper_token_t tok;

    // all expressions must start with "y=" (ignoring spaces)
    if (str[lex_index++] != 'y') return 0;
    while (str[lex_index] == ' ') lex_index++;
    if (str[lex_index++] != '=') return 0;

    while (str[lex_index]) {
        GET_NEXT_TOKEN(tok);
        if (variable && tok.toktype != TOK_OPEN_SQUARE
            && tok.toktype != TOK_OPEN_CURLY)
            variable = 0;
        switch (tok.toktype) {
            case TOK_CONST:
                // push to output stack
                PUSH_TO_OUTPUT(tok);
                variable = 0;
                break;
            case TOK_VAR:
                // set datatype
                tok.datatype = tok.var < VAR_Y ? input_type : output_type;
                tok.history_index = 0;
                tok.vector_index = 0;
                tok.vector_length = tok.var < VAR_Y ? input_vector_size : output_vector_size;
                tok.vector_length_locked = 1;
                PUSH_TO_OUTPUT(tok);
                variable = 1;
                break;
            case TOK_FUNC:
                if (function_table[tok.func].func_int32)
                    tok.datatype = 'i';
                else
                    tok.datatype = 'f';
                PUSH_TO_OPERATOR(tok);
                if (!function_table[tok.func].arity) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                break;
            case TOK_OPEN_PAREN:
                PUSH_TO_OPERATOR(tok);
                break;
            case TOK_CLOSE_PAREN:
                // pop from operator stack to output until left parenthesis found
                while (opstack_index >= 0 && opstack[opstack_index].toktype != TOK_OPEN_PAREN) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (opstack_index < 0)
                    {FAIL("Unmatched parentheses or misplaced comma.");}
                // remove left parenthesis from operator stack
                if (tok.toktype == TOK_CLOSE_PAREN) {
                    POP_OPERATOR();
                    if (opstack[opstack_index].toktype == TOK_FUNC) {
                        // if stack[top] is tok_func, pop to output
                        POP_OPERATOR_TO_OUTPUT();
                    }
                }
                break;
            case TOK_COMMA:
                // pop from operator stack to output until left parenthesis or TOK_VECTORIZE found
                while (opstack_index >= 0
                       && opstack[opstack_index].toktype != TOK_OPEN_PAREN
                       && opstack[opstack_index].toktype != TOK_VECTORIZE) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (opstack_index < 0)
                    {FAIL("Unmatched parentheses, brackets or misplaced comma.");}
                if (opstack[opstack_index].toktype == TOK_VECTORIZE) {
                    opstack[opstack_index].vector_index++;
                    opstack[opstack_index].vector_length +=
                        outstack[outstack_index].vector_length;
                    lock_vector_lengths(outstack, outstack_index);
                }
                break;
            case TOK_COLON:
                // pop from operator stack to output until conditional found
                while (opstack_index >= 0 &&
                       (opstack[opstack_index].toktype != TOK_OP ||
                        opstack[opstack_index].op != OP_CONDITIONAL_IF)) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (opstack_index < 0)
                    {FAIL("Unmatched colon.");}
                opstack[opstack_index].op = OP_CONDITIONAL_IF_ELSE;
                break;
            case TOK_QUESTION:
                tok.toktype = TOK_OP;
                tok.op = OP_CONDITIONAL_IF;
            case TOK_OP:
                // check precedence of operators on stack
                while (opstack_index >= 0 && opstack[opstack_index].toktype == TOK_OP
                       && op_table[opstack[opstack_index].op].precedence >=
                       op_table[tok.op].precedence) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                PUSH_TO_OPERATOR(tok);
                break;
            case TOK_OPEN_SQUARE:
                if (variable) {
                    GET_NEXT_TOKEN(tok);
                    if (tok.toktype != TOK_CONST || tok.datatype != 'i')
                        {FAIL("Non-integer vector index.");}
                    if (outstack[outstack_index].var < VAR_Y) {
                        if (tok.i >= input_vector_size)
                            {FAIL("Index exceeds input vector length.");}
                    }
                    else if (tok.i >= output_vector_size)
                        {FAIL("Index exceeds output vector length.");}
                    outstack[outstack_index].vector_index = tok.i;
                    outstack[outstack_index].vector_length = 1;
                    outstack[outstack_index].vector_length_locked = 1;
                    GET_NEXT_TOKEN(tok);
                    if (tok.toktype == TOK_COLON) {
                        // index is range A:B
                        GET_NEXT_TOKEN(tok);
                        if (tok.toktype != TOK_CONST || tok.datatype != 'i')
                            {FAIL("Malformed vector index.");}
                        if (outstack[outstack_index].var < VAR_Y) {
                            if (tok.i >= input_vector_size)
                                {FAIL("Index exceeds vector length.");}
                        }
                        else if (tok.i >= output_vector_size)
                            {FAIL("Index exceeds vector length.");}
                        if (tok.i <= outstack[outstack_index].vector_index)
                            {FAIL("Malformed vector index.");}
                        outstack[outstack_index].vector_length =
                            tok.i - outstack[outstack_index].vector_index + 1;
                        GET_NEXT_TOKEN(tok);
                    }
                    if (tok.toktype != TOK_CLOSE_SQUARE)
                        {FAIL("Unmatched bracket.");}
                }
                else {
                    if (vectorizing)
                        {FAIL("Nested (multidimensional) vectors not allowed.");}
                    tok.toktype = TOK_VECTORIZE;
                    tok.vector_length = 0;
                    // use vector index for tracking arity of tok
                    tok.vector_index = 0;
                    PUSH_TO_OPERATOR(tok);
                    vectorizing = 1;
                }
                break;
            case TOK_CLOSE_SQUARE:
                // pop from operator stack to output until TOK_VECTORIZE found
                while (opstack_index >= 0 &&
                       opstack[opstack_index].toktype != TOK_VECTORIZE) {
                    POP_OPERATOR_TO_OUTPUT();
                }
                if (opstack_index < 0)
                    {FAIL("Unmatched brackets or misplaced comma.");}
                opstack[opstack_index].vector_length_locked = 1;
                opstack[opstack_index].vector_index++;
                opstack[opstack_index].vector_length +=
                    outstack[outstack_index].vector_length;
                lock_vector_lengths(outstack, outstack_index);
                POP_OPERATOR_TO_OUTPUT();
                vectorizing = 0;
                break;
            case TOK_OPEN_CURLY:
                if (!variable)
                    {FAIL("Misplaced brace.");}
                GET_NEXT_TOKEN(tok);
                if (tok.toktype == TOK_NEGATE) {
                    // if negative sign found, get next token
                    outstack[outstack_index].history_index = -1;
                    GET_NEXT_TOKEN(tok);
                }
                if (tok.toktype != TOK_CONST || tok.datatype != 'i')
                    {FAIL("Non-integer history index.");}
                outstack[outstack_index].history_index *= tok.i;
                if (outstack[outstack_index].var == VAR_X) {
                    if (outstack[outstack_index].history_index > 0)
                        {FAIL("Input history index cannot be > 0.");}
                }
                else if (outstack[outstack_index].var == VAR_Y) {
                    if (outstack[outstack_index].history_index > -1)
                    {FAIL("Output history index cannot be > -1.");}
                }

                if (outstack[outstack_index].var == VAR_X
                    && outstack[outstack_index].history_index < oldest_input)
                    oldest_input = outstack[outstack_index].history_index;
                else if (outstack[outstack_index].var == VAR_Y
                         && outstack[outstack_index].history_index < oldest_output)
                    oldest_output = outstack[outstack_index].history_index;
                GET_NEXT_TOKEN(tok);
                if (tok.toktype != TOK_CLOSE_CURLY)
                    {FAIL("Unmatched brace.");}
                break;
            case TOK_NEGATE:
                // push '0' to output stack, and '-' to operator stack
                tok.toktype = TOK_CONST;
                tok.datatype = 'i';
                tok.i = 0;
                PUSH_TO_OUTPUT(tok);
                tok.toktype = TOK_OP;
                tok.op = OP_SUBTRACT;
                PUSH_TO_OPERATOR(tok);
                break;
            default:
                {FAIL("Unknown token type.");}
                break;
        }
#if TRACING
        printstack("OUTPUT STACK:", outstack, outstack_index);
        printstack("OPERATOR STACK:", opstack, opstack_index);
#endif
    }

    // finish popping operators to output, check for unbalanced parentheses
    while (opstack_index >= 0) {
        if (opstack[opstack_index].toktype == TOK_OPEN_PAREN)
            {FAIL("Unmatched parentheses or misplaced comma.");}
        POP_OPERATOR_TO_OUTPUT();
    }

    // Fail if top vector length doesn't match output vector length
    if (outstack[outstack_index].vector_length != output_vector_size)
        {FAIL("Expression vector length does not match destination.");}

    // Check for maximum vector length used in stack
    int i;
    for (i = 0; i < outstack_index; i++) {
        if (outstack[i].vector_length > max_vector)
            max_vector = outstack[i].vector_length;
    }

    // If stack top type doesn't match output type, add cast
    if (outstack[outstack_index].datatype != output_type) {
        promote_token_datatype(&outstack[outstack_index], output_type);
        // TODO: checking again shouldn't be necessary
        outstack_index = check_types_and_lengths(outstack, outstack_index);
        if (outstack[outstack_index].datatype != output_type)
            outstack[outstack_index].casttype = output_type;
    }

    mapper_expr expr = malloc(sizeof(struct _mapper_expr));
    expr->length = outstack_index + 1;
    expr->start = malloc(sizeof(struct _token)*expr->length);
    memcpy(expr->start, &outstack, sizeof(struct _token)*expr->length);
    expr->vector_size = max_vector;
    expr->input_history_size = *input_history_size = -oldest_input+1;
    expr->output_history_size = *output_history_size = -oldest_output+1;
    return expr;
}

int mapper_expr_input_history_size(mapper_expr expr)
{
    return expr->input_history_size;
}

int mapper_expr_output_history_size(mapper_expr expr)
{
    return expr->output_history_size;
}

#if TRACING
static void print_stack_vector(mapper_signal_value_t *stack, char type,
                               int vector_length)
{
    int i;
    if (vector_length > 1)
        printf("[");
    switch (type) {
        case 'i':
            for (i = 0; i < vector_length; i++)
                printf("%d, ", stack[i].i32);
            break;
        case 'f':
            for (i = 0; i < vector_length; i++)
                printf("%f, ", stack[i].f);
            break;
        case 'd':
            for (i = 0; i < vector_length; i++)
                printf("%f, ", stack[i].d);
            break;
        default:
            break;
    }
    if (vector_length > 1)
        printf("\b\b]");
    else
        printf("\b\b");
}
#endif

int mapper_expr_evaluate(mapper_expr expr,
                         mapper_signal_history_t *from,
                         mapper_signal_history_t *to)
{
    mapper_signal_value_t stack[expr->length][expr->vector_size];
    int dims[expr->length];

    int i, j, k, top = -1, count = 0;
    mapper_token_t *tok = expr->start;

    while (count < expr->length && tok->toktype != TOK_END) {
        switch (tok->toktype) {
        case TOK_CONST:
            ++top;
            dims[top] = tok->vector_length;
            if (tok->datatype == 'f') {
                for (i = 0; i < tok->vector_length; i++)
                    stack[top][i].f = tok->f;
            }
            else if (tok->datatype == 'd') {
                for (i = 0; i < tok->vector_length; i++)
                    stack[top][i].d = tok->d;
            }
            else if (tok->datatype == 'i') {
                for (i = 0; i < tok->vector_length; i++)
                    stack[top][i].i32 = tok->i;
            }
            break;
        case TOK_VAR:
            {
                int idx;
                switch (tok->var) {
                case VAR_X:
                    ++top;
                    dims[top] = tok->vector_length;
                    idx = ((tok->history_index + from->position
                            + from->size) % from->size);
                    if (from->type == 'd') {
                        double *v = from->value + idx * from->length * mapper_type_size(from->type);
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = v[i+tok->vector_index];
                    }
                    else if (from->type == 'f') {
                        float *v = from->value + idx * from->length * mapper_type_size(from->type);
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = v[i+tok->vector_index];
                    }
                    else if (from->type == 'i') {
                        int *v = from->value + idx * from->length * mapper_type_size(from->type);
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = v[i+tok->vector_index];
                    }
                    break;
                case VAR_Y:
                    ++top;
                    dims[top] = tok->vector_length;
                    idx = ((tok->history_index + to->position + 1
                            + to->size) % to->size);
                    if (to->type == 'd') {
                        double *v = to->value + idx * to->length * mapper_type_size(to->type);
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = v[i+tok->vector_index];
                    }
                    else if (to->type == 'f') {
                        float *v = to->value + idx * to->length * mapper_type_size(to->type);
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = v[i+tok->vector_index];
                    }
                    else if (to->type == 'i') {
                        int *v = to->value + idx * to->length * mapper_type_size(to->type);
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = v[i+tok->vector_index];
                    }
                    break;
                default: goto error;
                }
            }
            break;
        case TOK_OP:
            top -= op_table[tok->op].arity-1;
            dims[top] = tok->vector_length;
#if TRACING
            if (tok->op == OP_CONDITIONAL_IF || tok->op == OP_CONDITIONAL_IF_ELSE) {
                printf("IF ");
                print_stack_vector(stack[top], tok->datatype, tok->vector_length);
                printf(" THEN ");
                print_stack_vector(stack[top+1], tok->datatype, tok->vector_length);
                if (tok->op == OP_CONDITIONAL_IF_ELSE) {
                    printf(" ELSE ");
                    print_stack_vector(stack[top+2], tok->datatype, tok->vector_length);
                }
            }
            else {
                print_stack_vector(stack[top], tok->datatype, tok->vector_length);
                printf(" %s%c ", op_table[tok->op].name, tok->datatype);
                print_stack_vector(stack[top+1], tok->datatype, tok->vector_length);
            }
#endif
            if (tok->datatype == 'f') {
                switch (tok->op) {
                    case OP_ADD:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f + stack[top+1][i].f;
                        break;
                    case OP_SUBTRACT:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f - stack[top+1][i].f;
                        break;
                    case OP_MULTIPLY:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f * stack[top+1][i].f;
                        break;
                    case OP_DIVIDE:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f / stack[top+1][i].f;
                        break;
                    case OP_MODULO:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = fmod(stack[top][i].f, stack[top+1][i].f);
                        break;
                    case OP_IS_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f == stack[top+1][i].f;
                        break;
                    case OP_IS_NOT_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f != stack[top+1][i].f;
                        break;
                    case OP_IS_LESS_THAN:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f < stack[top+1][i].f;
                        break;
                    case OP_IS_LESS_THAN_OR_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f <= stack[top+1][i].f;
                        break;
                    case OP_IS_GREATER_THAN:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f > stack[top+1][i].f;
                        break;
                    case OP_IS_GREATER_THAN_OR_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f >= stack[top+1][i].f;
                        break;
                    case OP_LOGICAL_AND:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f && stack[top+1][i].f;
                        break;
                    case OP_LOGICAL_OR:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = stack[top][i].f || stack[top+1][i].f;
                        break;
                    case OP_LOGICAL_NOT:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = !stack[top][i].f;
                        break;
                    case OP_CONDITIONAL_IF:
                        // TODO: should not permit implicit any()/all()
                        for (i = 0; i < tok->vector_length; i++) {
                            if (stack[top][i].f)
                                stack[top][i].f = stack[top+1][i].f;
                            else
                                return 0;
                        }
                        break;
                    case OP_CONDITIONAL_IF_ELSE:
                        for (i = 0; i < tok->vector_length; i++) {
                            if (stack[top][i].f)
                                stack[top][i].f = stack[top+1][i].f;
                            else
                                stack[top][i].f = stack[top+2][i].f;
                        }
                        break;
                    default: goto error;
                }
            } else if (tok->datatype == 'd') {
                switch (tok->op) {
                    case OP_ADD:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d + stack[top+1][i].d;
                        break;
                    case OP_SUBTRACT:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d - stack[top+1][i].d;
                        break;
                    case OP_MULTIPLY:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d * stack[top+1][i].d;
                        break;
                    case OP_DIVIDE:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d / stack[top+1][i].d;
                        break;
                    case OP_MODULO:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = fmod(stack[top][i].d, stack[top+1][i].d);
                        break;
                    case OP_IS_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d == stack[top+1][i].d;
                        break;
                    case OP_IS_NOT_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d != stack[top+1][i].d;
                        break;
                    case OP_IS_LESS_THAN:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d < stack[top+1][i].d;
                        break;
                    case OP_IS_LESS_THAN_OR_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d <= stack[top+1][i].d;
                        break;
                    case OP_IS_GREATER_THAN:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d > stack[top+1][i].d;
                        break;
                    case OP_IS_GREATER_THAN_OR_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d >= stack[top+1][i].d;
                        break;
                    case OP_LOGICAL_AND:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d && stack[top+1][i].d;
                        break;
                    case OP_LOGICAL_OR:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = stack[top][i].d || stack[top+1][i].d;
                        break;
                    case OP_LOGICAL_NOT:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = !stack[top][i].d;
                        break;
                    case OP_CONDITIONAL_IF:
                        for (i = 0; i < tok->vector_length; i++) {
                            if (stack[top][i].d)
                                stack[top][i].d = stack[top+1][i].d;
                            else
                                return 0;
                        }
                        break;
                    case OP_CONDITIONAL_IF_ELSE:
                        for (i = 0; i < tok->vector_length; i++) {
                            if (stack[top][i].d)
                                stack[top][i].d = stack[top+1][i].d;
                            else
                                stack[top][i].d = stack[top+2][i].d;
                        }
                        break;
                    default: goto error;
                }
            } else {
                switch (tok->op) {
                    case OP_ADD:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 + stack[top+1][i].i32;
                        break;
                    case OP_SUBTRACT:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 - stack[top+1][i].i32;
                        break;
                    case OP_MULTIPLY:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 * stack[top+1][i].i32;
                        break;
                    case OP_DIVIDE:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 / stack[top+1][i].i32;
                        break;
                    case OP_MODULO:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 % stack[top+1][i].i32;
                        break;
                    case OP_IS_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 == stack[top+1][i].i32;
                        break;
                    case OP_IS_NOT_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 != stack[top+1][i].i32;
                        break;
                    case OP_IS_LESS_THAN:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 < stack[top+1][i].i32;
                        break;
                    case OP_IS_LESS_THAN_OR_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 <= stack[top+1][i].i32;
                        break;
                    case OP_IS_GREATER_THAN:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 > stack[top+1][i].i32;
                        break;
                    case OP_IS_GREATER_THAN_OR_EQUAL:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 >= stack[top+1][i].i32;
                        break;
                    case OP_LEFT_BIT_SHIFT:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 << stack[top+1][i].i32;
                        break;
                    case OP_RIGHT_BIT_SHIFT:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 >> stack[top+1][i].i32;
                        break;
                    case OP_BITWISE_AND:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 & stack[top+1][i].i32;
                        break;
                    case OP_BITWISE_OR:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 | stack[top+1][i].i32;
                        break;
                    case OP_BITWISE_XOR:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 ^ stack[top+1][i].i32;
                        break;
                    case OP_LOGICAL_AND:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 && stack[top+1][i].i32;
                        break;
                    case OP_LOGICAL_OR:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = stack[top][i].i32 || stack[top+1][i].i32;
                        break;
                    case OP_LOGICAL_NOT:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = !stack[top][i].i32;
                        break;
                    case OP_CONDITIONAL_IF:
                        for (i = 0; i < tok->vector_length; i++) {
                            if (stack[top][i].i32)
                                stack[top][i].i32 = stack[top+1][i].i32;
                            else
                                return 0;
                        }
                        break;
                    case OP_CONDITIONAL_IF_ELSE:
                        for (i = 0; i < tok->vector_length; i++) {
                            if (stack[top][i].i32)
                                stack[top][i].i32 = stack[top+1][i].i32;
                            else
                                stack[top][i].i32 = stack[top+2][i].i32;
                        }
                        break;
                    default: goto error;
                }
            }
#if TRACING
            printf(" = ");
            print_stack_vector(stack[top], tok->datatype, tok->vector_length);
            printf(" \n");
#endif
            break;
        case TOK_FUNC:
            top -= function_table[tok->func].arity-1;
            dims[top] = tok->vector_length;
#if TRACING
            printf("%s%c(", function_table[tok->func].name, tok->datatype);
            for (i = 0; i < function_table[tok->func].arity; i++) {
                print_stack_vector(stack[top], tok->datatype, tok->vector_length);
                printf(", ");
            }
            printf("\b\b)\n");
#endif
            if (tok->datatype == 'f') {
                switch (function_table[tok->func].arity) {
                    case 0:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = ((func_float_arity0*)function_table[tok->func].func_float)();
                        break;
                    case 1:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = ((func_float_arity1*)function_table[tok->func].func_float)(stack[top][i].f);
                        break;
                    case 2:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].f = ((func_float_arity2*)function_table[tok->func].func_float)(stack[top][i].f, stack[top+1][i].f);
                        break;
                    default: goto error;
                }
            }
            else if (tok->datatype == 'd') {
                switch (function_table[tok->func].arity) {
                    case 0:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = ((func_double_arity0*)function_table[tok->func].func_double)();
                        break;
                    case 1:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = ((func_double_arity1*)function_table[tok->func].func_double)(stack[top][i].d);
                        break;
                    case 2:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].d = ((func_double_arity2*)function_table[tok->func].func_double)(stack[top][i].d, stack[top+1][i].d);
                        break;
                    default: goto error;
                }
            }
            else if (tok->datatype == 'i') {
                switch (function_table[tok->func].arity) {
                    case 0:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = ((func_int32_arity0*)function_table[tok->func].func_int32)();
                        break;
                    case 1:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = ((func_int32_arity1*)function_table[tok->func].func_int32)(stack[top][i].i32);
                        break;
                    case 2:
                        for (i = 0; i < tok->vector_length; i++)
                            stack[top][i].i32 = ((func_int32_arity2*)function_table[tok->func].func_int32)(stack[top][i].i32, stack[top+1][i].i32);
                        break;
                    default: goto error;
                }
            }
#if TRACING
            printf(" = ");
            print_stack_vector(stack[top], tok->datatype, tok->vector_length);
            printf(" \n");
#endif
            break;
        case TOK_VECTORIZE:
            // don't need to copy vector elements from first token
            top -= tok->vector_index-1;
            k = dims[top];
            if (tok->datatype == 'f') {
                for (i = 1; i < tok->vector_index; i++) {
                    for (j = 0; j < dims[top+1]; j++)
                        stack[top][k++].f = stack[top+i][j].f;
                }
            }
            else if (tok->datatype == 'i') {
                for (i = 1; i < tok->vector_index; i++) {
                    for (j = 0; j < dims[top+1]; j++)
                        stack[top][k++].i32 = stack[top+i][j].i32;
                }
            }
            else if (tok->datatype == 'd') {
                for (i = 1; i < tok->vector_index; i++) {
                    for (j = 0; j < dims[top+1]; j++)
                        stack[top][k++].d = stack[top+i][j].d;
                }
            }
            dims[top] = tok->vector_length;
#if TRACING
            printf("built %i-element vector: ", tok->vector_length);
            print_stack_vector(stack[top], tok->datatype, tok->vector_length);
            printf(" \n");
#endif
            break;
        default: goto error;
        }
        if (tok->casttype) {
#if TRACING
            printf("casting from %c to %c\n", tok->datatype, tok->casttype);
#endif
            // need to cast to a different type
            if (tok->datatype == 'i') {
                if (tok->casttype == 'f') {
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].f = (float)stack[top][i].i32;
                    }
                }
                else if (tok->casttype == 'd') {
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].d = (double)stack[top][i].i32;
                    }
                }
            }
            else if (tok->datatype == 'f') {
                if (tok->casttype == 'i') {
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].i32 = (int)stack[top][i].f;
                    }
                }
                else if (tok->casttype == 'd') {
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].d = (double)stack[top][i].f;
                    }
                }
            }
            else if (tok->datatype == 'd') {
                if (tok->casttype == 'i') {
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].i32 = (int)stack[top][i].d;
                    }
                }
                else if (tok->casttype == 'f') {
                    for (i = 0; i < tok->vector_length; i++) {
                        stack[top][i].f = (float)stack[top][i].d;
                    }
                }
            }
        }
        tok++;
        count++;
    }

    /* Increment index position of output data structure.
     * We do this after computation to handle conditional output. */
    to->position = (to->position + 1) % to->size;

    if (to->type == 'f') {
        float *v = msig_history_value_pointer(*to);
        for (i = 0; i < to->length; i++)
            v[i] = stack[top][i].f;
    }
    else if (to->type == 'i') {
        int *v = msig_history_value_pointer(*to);
        for (i = 0; i < to->length; i++)
            v[i] = stack[top][i].i32;
    }
    else if (to->type == 'd') {
        double *v = msig_history_value_pointer(*to);
        for (i = 0; i < to->length; i++)
            v[i] = stack[top][i].d;
    }
    return 1;

  error:
    trace("Unexpected token in expression.");
    return 0;
}
