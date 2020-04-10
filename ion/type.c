typedef enum TypeKind
{
    TYPE_NONE,
    TYPE_INCOMPLETE,
    TYPE_COMPLETING,
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_SCHAR,
    TYPE_UCHAR,
    TYPE_SHORT,
    TYPE_USHORT,
    TYPE_INT,
    TYPE_UINT,
    TYPE_LONG,
    TYPE_ULONG,
    TYPE_LLONG,
    TYPE_ULLONG,
    TYPE_ENUM,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_PTR,
    TYPE_FUNC,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_CONST,
    NUM_TYPE_KINDS,
} TypeKind;

typedef struct Type Type;
typedef struct Sym Sym;

typedef struct TypeField
{
    const char* name;
    Type* type;
    size_t offset;
} TypeField;

struct Type
{
    TypeKind kind;
    size_t size;
    size_t align;
    Sym* sym;
    Type* base;
    bool nonmodifiable;
    union
    {
        size_t num_elems;
        struct
        {
            TypeField* fields;
            size_t num_fields;
        } aggregate;
        struct
        {
            Type** params;
            size_t num_params;
            bool has_varargs;
            Type* ret;
        } func;
    };
};

void vomplete_type(Type* type);

Type* type_alloc(TypeKind kind)
{
    Type* type = xcalloc
    // TODO day 17
}