typedef enum SymKind {
    SYM_NONE,
    SYM_VAR,
    SYM_CONST,
    SYM_FUNC,
    SYM_TYPE,
} SymKind;

typedef enum SymState {
    SYM_UNRESOLVED,
    SYM_RESOLVING,
    SYM_RESOLVED,
} SymState;

typedef struct Sym {
    const char *name;
    SymKind kind;
    SymState state;
    Decl *decl;
    Type *type;
    Val val;
} Sym;

enum {
    MAX_LOCAL_SYMS = 1024
};

Sym** sorted_syms;
Map global_syms_map;
Sym **global_syms_buf;
Sym local_syms[MAX_LOCAL_SYMS];
Sym *local_syms_end = local_syms;

Sym *sym_new(SymKind kind, const char *name, Decl *decl) {
    Sym *sym = xcalloc(1, sizeof(Sym));
    sym->kind = kind;
    sym->name = name;
    sym->decl = decl;
    return sym;
}

Sym *sym_decl(Decl *decl) {
    SymKind kind = SYM_NONE;
    switch (decl->kind) {
    case DECL_STRUCT:
    case DECL_UNION:
    case DECL_TYPEDEF:
    case DECL_ENUM:
        kind = SYM_TYPE;
        break;
    case DECL_VAR:
        kind = SYM_VAR;
        break;
    case DECL_CONST:
        kind = SYM_CONST;
        break;
    case DECL_FUNC:
        kind = SYM_FUNC;
        break;
    default:
        assert(0);
        break;
    }
    Sym *sym = sym_new(kind, decl->name, decl);
    if (decl->kind == DECL_STRUCT || decl->kind == DECL_UNION) {
        sym->state = SYM_RESOLVED;
        sym->type = type_incomplete(sym);
    }
    return sym;
}

Sym *sym_get_local(const char *name) {
    for (Sym *it = local_syms_end; it != local_syms; it--) {
        Sym *sym = it-1;
        if (sym->name == name) {
            return sym;
        }
    }
    return NULL;
}

Sym* sym_get(const char* name)
{
    Sym* sym = sym_get_local(name);
    return sym ? sym : map_get(&global_syms_map, (void*)name);
}

bool sym_push_var(const char *name, Type *type) {
    if (sym_get_local(name))
    {
        return false;
    }
    if (local_syms_end == local_syms + MAX_LOCAL_SYMS) {
        fatal("Too many local symbols");
    }
    *local_syms_end++ = (Sym){
        .name = name,
        .kind = SYM_VAR,
        .state = SYM_RESOLVED,
        .type = type,
    };
    return true;
}

Sym *sym_enter(void) {
    return local_syms_end;
}

void sym_leave(Sym *sym) {
    local_syms_end = sym;
}

void sym_global_put(Sym *sym) {
    if (map_get(&global_syms_map, (void*)sym->name))
    {
        SrcPos pos = sym->decl ? sym->decl->pos : pos_builtin;
        fatal_error(pos, "Duplicate definiton of global symbol");
    }
    map_put(&global_syms_map, (void *)sym->name, sym);
    buf_push(global_syms_buf, sym);
}

void sym_global_type(const char* name, Type* type)
{
    Sym* sym = sym_new(SYM_TYPE, str_intern(name), NULL);
    sym->state = SYM_RESOLVED;
    sym->type = type;
    sym_global_put(sym);
}

void sym_global_typedef(const char *name, Type *type) {
    Sym *sym = sym_new(SYM_TYPE, str_intern(name), decl_typedef(pos_builtin, name, typespec_name(pos_builtin, name)));
    sym->state = SYM_RESOLVED;
    sym->type = type;
    sym_global_put(sym);
}

void sym_global_const(const char* name, Type* type, Val val)
{
    Sym* sym = sym_new(SYM_CONST, str_intern(name), NULL);
    sym->state = SYM_RESOLVED;
    sym->type = type;
    sym->val = val;
    sym_global_put(sym);
}

void sym_global_func(const char *name, Type *type) {
    assert(type->kind == TYPE_FUNC);
    Sym *sym = sym_new(SYM_FUNC, str_intern(name), NULL);
    sym->state = SYM_RESOLVED;
    sym->type = type;
    sym_global_put(sym);
}

Sym* sym_global_decl(Decl* decl)
{
    Sym* sym = sym_decl(decl);
    sym_global_put(sym);
    decl->sym = sym;
    if (decl->kind == DECL_ENUM)
    {
        sym->state = SYM_RESOLVED;
        sym->type = type_enum(sym);
        buf_push(sorted_syms, sym);
        for (int i = 0; i < decl->enum_decl.num_items; ++i)
        {
            EnumItem item = decl->enum_decl.items[i];
            if (item.init)
            {
                fatal_error(item.pos, "Explicit enum constant initializers are not currently supported");
            }
            sym_global_const(item.name, sym->type, (Val){.i = i});
        }
    }
    return sym;
}

typedef struct Operand {
    Type *type;
    bool is_lvalue;
    bool is_const;
    Val val;
} Operand;

Operand operand_null;

Operand operand_rvalue(Type *type) {
    return (Operand){
        .type = unqualify_type(type),
    };
}

Operand operand_lvalue(Type *type) {
    return (Operand){
        .type = type,
        .is_lvalue = true,
    };
}

Operand operand_const(Type *type, Val val) {
    return (Operand){
        .type = unqualify_type(type),
        .is_const = true,
        .val = val,
    };
}

Operand operand_decay(Operand operand)
{
    operand.type = unqualify_type(operand.type);
    if (operand.type->kind == TYPE_ARRAY)
    {
        operand.type = type_ptr(operand.type->base);
    }
    operand.is_lvalue = false;
    return operand;
}

bool is_null_ptr(Operand operand);

#define CASE(k, t) \
    case k: \
        switch (type->kind) { \
        case TYPE_BOOL: \
            operand->val.b = (bool)operand->val.t; \
            break; \
        case TYPE_CHAR: \
            operand->val.c = (char)operand->val.t; \
            break; \
        case TYPE_UCHAR: \
            operand->val.uc = (unsigned char)operand->val.t; \
            break; \
        case TYPE_SCHAR: \
            operand->val.sc = (signed char)operand->val.t; \
            break; \
        case TYPE_SHORT: \
            operand->val.s = (short)operand->val.t; \
            break; \
        case TYPE_USHORT: \
            operand->val.us = (unsigned short)operand->val.t; \
            break; \
        case TYPE_INT: \
        case TYPE_ENUM: \
            operand->val.i = (int)operand->val.t; \
            break; \
        case TYPE_UINT: \
            operand->val.u = (unsigned)operand->val.t; \
            break; \
        case TYPE_LONG: \
            operand->val.l = (long)operand->val.t; \
            break; \
        case TYPE_ULONG: \
            operand->val.ul = (unsigned long)operand->val.t; \
            break; \
        case TYPE_LLONG: \
            operand->val.ll = (long long)operand->val.t; \
            break; \
        case TYPE_ULLONG: \
            operand->val.ull = (unsigned long long)operand->val.t; \
            break; \
        case TYPE_PTR: \
            operand->val.p = (uintptr_t)operand->val.t; \
            break; \
        case TYPE_DOUBLE: \
        case TYPE_DOUBLE: \
            break; \
        default: \
            operand->is_const = false; \
            break; \
        } \
        break;

bool is_convertible(Operand* operand, Type* dest)
{
    dest = unqualify_type(dest);
    Type* src = unqualify_type(operand->type);
    if (dest == src)
    {
        return true;
    }
    else if (is_arithmetic_type(dest) && is_arithmetic_type(src))
    {
        return true;
    }
    else if (is_ptr_type(dest) && is_null_ptr(*operand))
    {
        return true;
    }
    else if (is_ptr_type(dest) && is_ptr_type(src))
    {
        if (is_const_type(dest->base) && is_const_type(src->base))
        {
            return dest->base->base == src->base->base || dest->base->base == type_void || src->base->base == type_void;
        }
        else
        {
            Type* unqual_dest_base = unqualify_type(dest->base);
            if (unqual_dest_base == src->base)
            {
                return true;
            }
            else if (unqual_dest_base == type_void)
            {
                return is_const_type(dest->base) || !is_const_type(src->base);
            }
            else
            {
                return ptr->base == type_void;
            }
        }
    }
    else
    {
        return false;
    }
}

// TODO continue here day 17
bool is_castable(Operand* operand, Type* dest)
    {
        return true;
    }
    if (!is_convertible(operand->type, type))
    {
        return false;
    }
    if (operand->is_const) {
        switch (operand->type->kind) {
        CASE(TYPE_CHAR, c)
        CASE(TYPE_UCHAR, uc)
        CASE(TYPE_SCHAR, sc)
        CASE(TYPE_SHORT, s)
        CASE(TYPE_USHORT, us)
        CASE(TYPE_INT, i)
        CASE(TYPE_UINT, u)
        CASE(TYPE_LONG, l)
        CASE(TYPE_ULONG, ul)
        CASE(TYPE_LLONG, ll)
        CASE(TYPE_ULLONG, ull)
        CASE(TYPE_FLOAT, f)
        CASE(TYPE_DOUBLE, d)
        default:
            operand->is_const = false;
            break;
        }
    }
    operand->type = type;
    return true;
}

#undef CASE

Val convert_val(Type *dest_type, Type *src_type, Val src_val) {
    Operand operand = operand_const(src_type, src_val);
    convert_operand(&operand, dest_type);
    return operand.val;
}

void promote_operand(Operand *operand) {
    switch (operand->type->kind) {
    case TYPE_CHAR:
    case TYPE_SCHAR:
    case TYPE_UCHAR:
    case TYPE_SHORT:
    case TYPE_USHORT:
        convert_operand(operand, type_int);
        break;
    default:
        // Do nothing
        break;
    }
}

Type *promote_type(Type *type) {
    Operand operand = operand_rvalue(type);
    promote_operand(&operand);
    return operand.type;
}

void unify_arithmetic_operands(Operand *left, Operand *right) {
    if (left->type == type_double) {
        convert_operand(right, type_double);
    } else if (right->type == type_double) {
        convert_operand(left, type_double);
    } else if (left->type == type_float) {
        convert_operand(right, type_float);
    } else if (right->type == type_float) {
        convert_operand(left, type_float);
    } else {
        assert(is_integer_type(left->type));
        assert(is_integer_type(right->type));
        promote_operand(left);
        promote_operand(right);
        if (left->type != right->type) {
            if (is_signed_type(left->type) == is_signed_type(right->type)) {
                if (type_rank(left->type) <= type_rank(right->type)) {
                    convert_operand(left, right->type);
                } else {
                    convert_operand(right, left->type);
                }
            } else if (is_signed_type(left->type) && type_rank(right->type) >= type_rank(left->type)) {
                convert_operand(left, right->type);
            } else if (is_signed_type(right->type) && type_rank(left->type) >= type_rank(right->type)) {
                convert_operand(right, left->type);
            } else if (is_signed_type(left->type) && type_sizeof(left->type) > type_sizeof(right->type)) {
                convert_operand(right, left->type);            
            } else if (is_signed_type(right->type) && type_sizeof(right->type) > type_sizeof(left->type)) {
                convert_operand(left, right->type);
            } else { 
                Type *type = unsigned_type(is_signed_type(left->type) ? left->type : right->type);
                convert_operand(left, type);
                convert_operand(right, type);
            }
        }
    }
    assert(left->type == right->type);
}

Type *unify_arithmetic_types(Type *left_type, Type *right_type)
{
    Operand left = operand_rvalue(left_type);
    Operand right = operand_rvalue(right_type);
    unify_arithmetic_operands(&left, &right);
    assert(left.type == right.type);
    return left.type;
}

Sym *resolve_name(const char *name);
Operand resolve_const_expr(Expr *expr);
Operand resolve_expr(Expr *expr);
Operand resolve_expected_expr(Expr *expr, Type *expected_type);

Type *resolve_typespec(Typespec *typespec) {
    if (!typespec) {
        return type_void;
    }
    Type *result = NULL;
    switch (typespec->kind) {
    case TYPESPEC_NAME: {
        Sym *sym = resolve_name(typespec->name);
        if (sym->kind != SYM_TYPE) {
            fatal_error(typespec->pos, "%s must denote a type", typespec->name);
            return NULL;
        }
        result = sym->type;
        break;
    }
    case TYPESPEC_PTR:
        result = type_ptr(resolve_typespec(typespec->ptr.elem));
        break;
    case TYPESPEC_ARRAY: {
        int size = 0;
        if (typespec->array.size) {
            Operand operand = resolve_const_expr(typespec->array.size);
            if (!is_integer_type(operand.type))
            {
                fatal_error(typespec->pos, "Array size constant expression must have type int");
            }
            convert_operand(&operand, type_int);
            size = operand.val.i;
            if (size <= 0) {
                fatal_error(typespec->array.size->pos, "Non-positive array size");
            }
        }
        result = type_array(resolve_typespec(typespec->array.elem), size);
        break;
    }
    case TYPESPEC_FUNC: {
        Type **args = NULL;
        for (size_t i = 0; i < typespec->func.num_args; i++) {
            buf_push(args, resolve_typespec(typespec->func.args[i]));
        }
        Type *ret = type_void;
        if (typespec->func.ret) {
            ret = resolve_typespec(typespec->func.ret);
        }
        result = type_func(args, buf_len(args), ret, false);
        break;
    }
    default:
        assert(0);
        return NULL;
    }
    assert(!typespec->type || typespec->type == result);
    typespec->type = result;
    return result;
}

Sym **sorted_syms;

void complete_type(Type *type) {
    if (type->kind == TYPE_COMPLETING) {
        fatal_error(type->sym->decl->pos, "Type completion cycle");
        return;
    } else if (type->kind != TYPE_INCOMPLETE) {
        return;
    }
    Decl *decl = type->sym->decl;
    type->kind = TYPE_COMPLETING;
    assert(decl->kind == DECL_STRUCT || decl->kind == DECL_UNION);
    TypeField *fields = NULL;
    for (size_t i = 0; i < decl->aggregate.num_items; i++) {
        AggregateItem item = decl->aggregate.items[i];
        Type *item_type = resolve_typespec(item.type);
        complete_type(item_type);
        for (size_t j = 0; j < item.num_names; j++) {
            buf_push(fields, (TypeField){item.names[j], item_type});
        }
    }
    if (buf_len(fields) == 0) {
        fatal_error(decl->pos, "No fields");
    }
    if (duplicate_fields(fields, buf_len(fields))) {
        fatal_error(decl->pos, "Duplicate fields");
    }
    if (decl->kind == DECL_STRUCT) {
        type_complete_struct(type, fields, buf_len(fields));
    } else {
        assert(decl->kind == DECL_UNION);
        type_complete_union(type, fields, buf_len(fields));
    }
    buf_push(sorted_syms, type->sym);
}

Type *resolve_decl_type(Decl *decl) {
    assert(decl->kind == DECL_TYPEDEF);
    return resolve_typespec(decl->typedef_decl.type);
}

Type *resolve_decl_var(Decl *decl) {
    assert(decl->kind == DECL_VAR);
    Type *type = NULL;
    if (decl->var.type) {
        type = resolve_typespec(decl->var.type);
    }
    if (decl->var.expr) {
        Operand operand = resolve_expected_expr(decl->var.expr, type);
        if (type)
        {
            if (type->kind == TYPE_ARRAY && operand.type->kind == TYPE_ARRAY && type->array.elem == operand.type->array.elem && !type->array.size)
            {
                // Incomplete array size, so infer the size from the initializer expression's type.
            }
            else
            {
                if (!convert_operand(&operand, type))
                {
                    fatal_error(decl->pos, "Illegal conversion in variable initializer");
                }
            }
        }
        type = operand.type;
    }
    complete_type(type);
    return type;
}

Type *resolve_decl_const(Decl *decl, Val *val) {
    assert(decl->kind == DECL_CONST);
    Operand result = resolve_expr(decl->const_decl.expr);
    if (!result.is_const) {
        fatal_error(decl->pos, "Const initializer is not a constant expression");
    }
    *val = result.val;
    return result.type;
}

Type *resolve_decl_func(Decl *decl) {
    assert(decl->kind == DECL_FUNC);
    Type **params = NULL;
    for (size_t i = 0; i < decl->func.num_params; i++) {
        buf_push(params, resolve_typespec(decl->func.params[i].type));
    }
    Type *ret_type = type_void;
    if (decl->func.ret_type) {
        ret_type = resolve_typespec(decl->func.ret_type);
    }
    return type_func(params, buf_len(params), ret_type, decl->func.variadic);
}

void resolve_stmt(Stmt *stmt, Type *ret_type);

void resolve_cond_expr(Expr *expr)
{
    Operand cond = resolve_expr(expr);
    if (!is_arithmetic_type(cond.type) && cond.type->kind != TYPE_PTR)
    {
        fatal_error(expr->pos, "Conditional expression must have arithmetic or pointer type");
    }
}

void resolve_stmt_block(StmtList block, Type *ret_type) {
    Sym *scope = sym_enter();
    for (size_t i = 0; i < block.num_stmts; i++) {
        resolve_stmt(block.stmts[i], ret_type);
    }
    sym_leave(scope);
}

void resolve_stmt_assign(Stmt* stmt)
{
    assert(stmt->kind == STMT_ASSIGN);
    Operand left = resolve_expr(stmt->assign.left);
    if (!left.is_lvalue)
    {
        fatal_error(stmt->pos, "Cannot assign to non-lvalue");
    }
    if (stmt->assign.right)
    {
        Operand right = resolve_expected_expr(stmt->assign.right, left.type);
        if (!convert_operand(&right, left.type))
        {
            fatal_error(stmt->pos, "Illegal conversion in assignment statement");
        }
    }
}

void resolve_stmt(Stmt *stmt, Type *ret_type) {
    switch (stmt->kind) {
    case STMT_RETURN:
        if (stmt->expr)
        {
            Operand operand = resolve_expected_expr(stmt->expr, ret_type);
            if (!convert_operand(&operand, ret_type))
            {
                fatal_error(stmt->pos, "Illegal conversion in return expression");
            }
        }
        else if (ret_type != type_void)
        {
            fatal_error(stmt->pos, "Empty return expression for function with non-void return type");
        }
        break;
    case STMT_BREAK:
    case STMT_CONTINUE:
        // Do nothing
        break;
    case STMT_BLOCK:
        resolve_stmt_block(stmt->block, ret_type);
        break;
    case STMT_IF:
        resolve_cond_expr(stmt->if_stmt.cond);
        resolve_stmt_block(stmt->if_stmt.then_block, ret_type);
        for (size_t i = 0; i < stmt->if_stmt.num_elseifs; i++) {
            ElseIf elseif = stmt->if_stmt.elseifs[i];
            resolve_cond_expr(elseif.cond);
            resolve_stmt_block(elseif.block, ret_type);
        }
        if (stmt->if_stmt.else_block.stmts) {
            resolve_stmt_block(stmt->if_stmt.else_block, ret_type);
        }
        break;
    case STMT_WHILE:
    case STMT_DO_WHILE:
        resolve_cond_expr(stmt->while_stmt.cond);
        resolve_stmt_block(stmt->while_stmt.block, ret_type);
        break;
    case STMT_FOR: {
        Sym *scope = sym_enter();
        resolve_stmt(stmt->for_stmt.init, ret_type);
        resolve_cond_expr(stmt->for_stmt.cond);
        resolve_stmt_block(stmt->for_stmt.block, ret_type);
        resolve_stmt(stmt->for_stmt.next, ret_type);
        sym_leave(scope);
        break;
    }
    case STMT_SWITCH: {
        Operand expr = resolve_expr(stmt->switch_stmt.expr);
        for (size_t i = 0; i < stmt->switch_stmt.num_cases; i++) {
            SwitchCase switch_case = stmt->switch_stmt.cases[i];
            for (size_t j = 0; j < switch_case.num_exprs; j++)
            {
                Expr* case_expr = switch_case.exprs[j];
                Operand case_operand = resolve_expr(case_expr);
                if (!convert_operand(&case_operand, expr.type))
                {
                    fatal_error(case_expr->pos, "Illegal conversion in switch case expression");
                }
                resolve_stmt_block(switch_case.block, ret_type);
            }
        }
        break;
    }
    case STMT_ASSIGN:
    {
        resolve_stmt_assign(stmt);
        break;
    }
    case STMT_INIT:

        sym_push_var(stmt->init.name, resolve_expr(stmt->init.expr).type);
        break;
    case STMT_EXPR:
        resolve_expr(stmt->expr);
        break;
    default:
        assert(0);
        break;
    }
}

void resolve_func_body(Sym *sym) {
    Decl *decl = sym->decl;
    assert(decl->kind == DECL_FUNC);
    assert(sym->state == SYM_RESOLVED);
    Sym *scope = sym_enter();
    for (size_t i = 0; i < decl->func.num_params; i++) {
        FuncParam param = decl->func.params[i];
        sym_push_var(param.name, resolve_typespec(param.type));
    }
    resolve_stmt_block(decl->func.block, resolve_typespec(decl->func.ret_type));
    sym_leave(scope);
}

void resolve_sym(Sym *sym) {
    if (sym->state == SYM_RESOLVED) {
        return;
    } else if (sym->state == SYM_RESOLVING)
    {
        fatal_error(sym->decl->pos, "Cyclic dependency");
        return;
    }
    assert(sym->state == SYM_UNRESOLVED);
    sym->state = SYM_RESOLVING;
    switch (sym->kind) {
    case SYM_TYPE:
        sym->type = resolve_decl_type(sym->decl);
        break;
    case SYM_VAR:
        sym->type = resolve_decl_var(sym->decl);
        break;
    case SYM_CONST:
        sym->type = resolve_decl_const(sym->decl, &sym->val);
        break;
    case SYM_FUNC:
        sym->type = resolve_decl_func(sym->decl);
        break;
    default:
        assert(0);
        break;
    }
    sym->state = SYM_RESOLVED;
    buf_push(sorted_syms, sym);
}

void finalize_sym(Sym *sym) {
    resolve_sym(sym);
    if (sym->kind == SYM_TYPE) {
        complete_type(sym->type);
    } else if (sym->kind == SYM_FUNC) {
        resolve_func_body(sym);
    }
}

Sym *resolve_name(const char *name) {
    Sym *sym = sym_get(name);
    if (!sym) {
        return NULL;
    }
    resolve_sym(sym);
    return sym;
}

Operand resolve_expr_field(Expr *expr) {
    assert(expr->kind == EXPR_FIELD);
    Operand left = resolve_expr(expr->field.expr);
    Type *type = left.type;
    complete_type(type);
    if (type->kind != TYPE_STRUCT && type->kind != TYPE_UNION) {
        fatal_error(expr->pos, "Can only access fields on aggregate types");
        return operand_null;
    }
    for (size_t i = 0; i < type->aggregate.num_fields; i++) {
        TypeField field = type->aggregate.fields[i];
        if (field.name == expr->field.name) {
            return left.is_lvalue ? operand_lvalue(field.type) : operand_rvalue(field.type);
        }
    }
    fatal_error(expr->pos, "No field named '%s'", expr->field.name);
    return operand_null;
}

Operand ptr_decay(Operand expr) {
    if (expr.type->kind == TYPE_ARRAY) {
        return operand_rvalue(type_ptr(expr.type->array.elem));
    } else {
        return expr;
    }
}

long long eval_unary_op_ll(TokenKind op, long long val)
{
    switch (op)
    {
        case TOKEN_ADD:
        {
            return +val;
        } break;

        case TOKEN_SUB:
        {
            return -val;
        } break;

        case TOKEN_NEG:
        {
            return ~val;
        } break;

        case TOKEN_NOT:
        {
            return !val;
        } break;

        default:
        {
            assert(0);
        } break;
    }
    return 0;
}

unsigned long long eval_unary_op_ull(TokenKind op, unsigned long long val)
{
    switch (op)
    {
        case TOKEN_ADD:
        {
            return +val;
        } break;

        case TOKEN_SUB:
        {
            return 0ull - val; // Shut up MSVC's unary minus warning
        } break;

        case TOKEN_NEG:
        {
            return ~val;
        } break;

        case TOKEN_NOT:
        {
            return !val;
        } break;

        default:
        {
            assert(0);
        } break;
    }
    return 0;
}

long long eval_binary_op_ll(TokenKind op, long long left, long long right)
{
    switch (op)
    {
        case TOKEN_MUL:
        {
            return left * right;
        } break;

        case TOKEN_DIV:
        {
            return right != 0 ? left / right : 0;
        } break;

        case TOKEN_MOD:
        {
            return right != 0 ? left % right : 0;
        } break;

        case TOKEN_AND:
        {
            return left & right;
        } break;

        case TOKEN_LSHIFT:
        {
            return left << right;
        } break;

        case TOKEN_RSHIFT:
        {
            return left >> right;
        } break;

        case TOKEN_ADD:
        {
            return left + right;
        } break;

        case TOKEN_SUB:
        {
            return left - right;
        } break;

        case TOKEN_OR:
        {
            return left | right;
        } break;

        case TOKEN_XOR:
        {
            return left ^ right;
        } break;

        case TOKEN_EQ:
        {
            return left == right;
        } break;

        case TOKEN_NOTEQ:
        {
            return left != right;
        } break;

        case TOKEN_LT:
        {
            return left < right;
        } break;

        case TOKEN_LTEQ:
        {
            return left <= right;
        } break;

        case TOKEN_GT:
        {
            return left > right;
        } break;

        case TOKEN_GTEQ:
        {
            return left >= right;
        } break;

        case TOKEN_AND_AND:
        {
            return left && right;
        } break;

        case TOKEN_OR_OR:
        {
            return left || right;
        } break;

        default:
        {
            assert(0);
        } break;
    }
    return 0;
}

unsigned long long eval_binary_op_ull(TokenKind op, unsigned long long left, unsigned long long right)
{
    switch (op)
    {
        case TOKEN_MUL:
        {
            return left * right;
        } break;

        case TOKEN_DIV:
        {
            return right != 0 ? left / right : 0;
        } break;

        case TOKEN_MOD:
        {
            return right != 0 ? left % right : 0;
        } break;

        case TOKEN_AND:
        {
            return left & right;
        } break;

        case TOKEN_LSHIFT:
        {
            return left << right;
        } break;

        case TOKEN_RSHIFT:
        {
            return left >> right;
        } break;

        case TOKEN_ADD:
        {
            return left + right;
        } break;

        case TOKEN_SUB:
        {
            return left - right;
        } break;

        case TOKEN_OR:
        {
            return left | right;
        } break;

        case TOKEN_XOR:
        {
            return left ^ right;
        } break;

        case TOKEN_EQ:
        {
            return left == right;
        } break;

        case TOKEN_NOTEQ:
        {
            return left != right;
        } break;

        case TOKEN_LT:
        {
            return left < right;
        } break;

        case TOKEN_LTEQ:
        {
            return left <= right;
        } break;

        case TOKEN_GT:
        {
            return left > right;
        } break;

        case TOKEN_GTEQ:
        {
            return left >= right;
        } break;

        case TOKEN_AND_AND:
        {
            return left && right;
        } break;

        case TOKEN_OR_OR:
        {
            return left || right;
        } break;

        default:
        {
            assert(0);
        } break;
    }
    return 0;
}

Val eval_unary_op(TokenKind op, Type *type, Val val)
{
    Operand operand = operand_const(type, val);
    if (is_signed_type(type))
    {
        convert_operand(&operand, type_llong);
        operand.val.ll = eval_unary_op_ll(op, operand.val.ll);
    }
    else
    {
        convert_operand(&operand, type_ullong);
        operand.val.ull = eval_unary_op_ull(op, operand.val.ull);
    }
    convert_operand(&operand, type);
    return operand.val;
}

Val eval_binary_op(TokenKind op, Type *type, Val left, Val right)
{
    Operand left_operand = operand_const(type, left);
    Operand right_operand = operand_const(type, right);
    Operand result_operand;
    if (is_signed_type(type))
    {
        convert_operand(&left_operand, type_llong);
        convert_operand(&right_operand, type_llong);
        result_operand = operand_const(type_llong, (Val){.ll = eval_binary_op_ll(op, left_operand.val.ll, right_operand.val.ll)});
    }
    else
    {
        convert_operand(&left_operand, type_ullong);
        convert_operand(&right_operand, type_ullong);
        result_operand = operand_const(type_ullong, (Val){.ull = eval_binary_op_ull(op, left_operand.val.ull, right_operand.val.ull)});
    }
    convert_operand(&result_operand, type);
    return result_operand.val;
}

Operand resolve_expr_name(Expr *expr) {
    assert(expr->kind == EXPR_NAME);
    Sym *sym = resolve_name(expr->name);
    if (!sym)
    {
        fatal_error(expr->pos, "Name in expression does not exist");
    }
    if (sym->kind == SYM_VAR) {
        return operand_lvalue(sym->type);
    } else if (sym->kind == SYM_CONST) {
        return operand_const(sym->type, sym->val);
    } else if (sym->kind == SYM_FUNC) {
        return operand_rvalue(sym->type);
    } else {
        fatal_error(expr->pos, "%s must be a var or const", expr->name);
        return operand_null;
    }
}

Operand resolve_unary_op(TokenKind op, Operand operand)
{
    promote_operand(&operand);
    if (operand.is_const)
    {
        return operand_const(operand.type, eval_unary_op(op, operand.type, operand.val));
    }
    else
    {
        return operand;
    }
}
 
Operand resolve_expr_unary(Expr *expr) {
    assert(expr->kind == EXPR_UNARY);
    Operand operand = resolve_expr(expr->unary.expr);
    Type *type = operand.type;
    switch (expr->unary.op) {
    case TOKEN_MUL:
        operand = ptr_decay(operand);
        type = operand.type;
        if (type->kind != TYPE_PTR) {
            fatal_error(expr->pos, "Cannot deref non-ptr type");
        }
        return operand_lvalue(type->ptr.elem);
    case TOKEN_AND:
        if (!operand.is_lvalue) {
            fatal_error(expr->pos, "Cannot take address of non-lvalue");
        }
        return operand_rvalue(type_ptr(type));
    case TOKEN_ADD:
    case TOKEN_SUB:
    {
        if (!is_arithmetic_type(type))
        {
            fatal_error(expr->pos, "Can only use unary %s with arithmetic types", token_kind_name(expr->unary.op));
        }
        return resolve_unary_op(expr->unary.op, operand);
    } break;
    case TOKEN_NEG:
    {
        if (!is_integer_type(type))
        {
            fatal_error(expr->pos, "Can only use ~ with integer types", token_kind_name(expr->unary.op));
        }
        return resolve_unary_op(expr->unary.op, operand);
    } break;
    default:
    {
        assert(0);
    } break;
    return (Operand){0};
    }
}

Operand resolve_binary_op(TokenKind op, Operand left, Operand right)
{
    if (left.is_const && right.is_const)
    {
        return operand_const(left.type, eval_binary_op(op, left.type, left.val, right.val));
    }
    else
    {
        return operand_rvalue(left.type);
    }
}

Operand resolve_binary_arithmetic_op(TokenKind op, Operand left, Operand right)
{
    unify_arithmetic_operands(&left, &right);
    return resolve_binary_op(op, left, right);
}

Operand resolve_expr_binary(Expr *expr) {
    assert(expr->kind == EXPR_BINARY);
    Operand left = resolve_expr(expr->binary.left);
    Operand right = resolve_expr(expr->binary.right);
    TokenKind op = expr->binary.op;
    const char* op_name = token_kind_name(op);
    switch (op)
    {
        case TOKEN_MUL:
        case TOKEN_DIV:
        {
            if (!is_arithmetic_type(left.type))
            {
                fatal_error(expr->binary.left->pos, "Left operand of %s must have arithmetic type", op_name);
            }
            if (!is_arithmetic_type(right.type))
            {
                fatal_error(expr->binary.right->pos, "Right operand of %s must have arithmetic type", op_name);
            }
            return resolve_binary_arithmetic_op(op, left, right);
        } break;

        case TOKEN_MOD:
        {
            if (!is_integer_type(left.type))
            {
                fatal_error(expr->binary.left->pos, "Left operand of %% must have integer type");
            }
            if (!is_integer_type(right.type))
            {
                fatal_error(expr->binary.right->pos, "Right operand of %% must have integer type");
            }
            return resolve_binary_arithmetic_op(op, left, right);
        } break;

        case TOKEN_ADD:
        {
            if (is_arithmetic_type(left.type) && is_arithmetic_type(right.type))
            {
                return resolve_binary_arithmetic_op(op, left, right);
            }
            else if (left.type->kind == TYPE_PTR && is_integer_type(right.type))
            {
                return operand_rvalue(left.type);
            }
            else if (right.type->kind == TYPE_PTR && is_integer_type(left.type))
            {
                return operand_rvalue(right.type);
            }
            else
            {
                fatal_error(expr->pos, "Operands of + must both have arithmetic type, or pointer and integer type");
            }
        } break;

        case TOKEN_SUB:
        {
            if (is_arithmetic_type(left.type) && is_arithmetic_type(right.type))
            {
                return resolve_binary_arithmetic_op(op, left, right);
            }
            else if (left.type->kind == TYPE_PTR && is_integer_type(right.type))
            {
                return operand_rvalue(left.type);
            }
            else if (left.type->kind == TYPE_PTR && right.type->kind == TYPE_PTR)
            {
                if (left.type->ptr.elem != right.type->ptr.elem)
                {
                    fatal_error(expr->pos, "Cannot subtract pointers to different types");
                }
                return operand_rvalue(type_ssize);
            }
            else
            {
                fatal_error(expr->pos, "Operands of - must both have arithmetic type, pointer and integer type, or compatible pointer types");
            }
        } break;

        case TOKEN_LSHIFT:
        case TOKEN_RSHIFT:
        {
            if (is_integer_type(left.type) && is_integer_type(right.type))
            {
                promote_operand(&left);
                promote_operand(&right);
                Type* result_type = left.type;
                Operand result;
                if (is_signed_type(left.type))
                {
                    convert_operand(&left, type_llong);
                    convert_operand(&right, type_llong);
                }
                else
                {
                    convert_operand(&left, type_ullong);
                    convert_operand(&right, type_ullong);
                }
                result = resolve_binary_op(op, left, right);
                convert_operand(&result, result_type);
                return result;
            }
            else
            {
                fatal_error(expr->pos, "Operands of %s must both have integer type", op_name);
            }
        } break;

        case TOKEN_LT:
        case TOKEN_LTEQ:
        case TOKEN_GT:
        case TOKEN_GTEQ:
        case TOKEN_EQ:
        case TOKEN_NOTEQ:
        {
            if (is_arithmetic_type(left.type) && is_arithmetic_type(right.type))
            {
                Operand result = resolve_binary_arithmetic_op(op, left, right);
                convert_operand(&result, type_int);
                return result;
            }
            else if (left.type->kind == TYPE_PTR && right.type->kind == TYPE_PTR)
            {
                if (left.type->ptr.elem != right.type->ptr.elem)
                {
                    fatal_error(expr->pos, "Cannot compare points to different types");
                }
                return operand_rvalue(type_int);
            }
            else
            {
                // TODO(nicholas): Handle null pointer constants.
                fatal_error(expr->pos, "Operands of %s must be arithmetic types or compatible pointer types", op_name);
            }
        } break;

        case TOKEN_AND:
        case TOKEN_XOR:
        case TOKEN_OR:
        {
            if (is_integer_type(left.type) && is_integer_type(right.type))
            {
                return resolve_binary_arithmetic_op(op, left, right);
            }
            else
            {
                fatal_error(expr->pos, "Operands of %s must have arithmetic types", op_name);
            }
        } break;

        case TOKEN_AND_AND:
        case TOKEN_OR_OR:
        {
            // TODO(nicholas): const expr evaluation.
            if (is_scalar_type(left.type) && is_scalar_type(right.type))
            {
                return operand_rvalue(type_int);
            }
            else
            {
                fatal_error(expr->pos, "Operands of %s must have scalar types", op_name);
            }
        } break;

        default:
        {
            assert(0);
        } break;
    }
    return (Operand){0};
}

int aggregate_field_index(Type *type, const char *name) {
    assert(type->kind == TYPE_STRUCT || type->kind == TYPE_UNION);
    for (int i = 0; i < type->aggregate.num_fields; i++) {
        if (type->aggregate.fields[i].name == name) {
            return i;
        }
    }
    return -1;
}

Operand resolve_expr_compound(Expr *expr, Type *expected_type) {
    assert(expr->kind == EXPR_COMPOUND);
    if (!expected_type && !expr->compound.type) {
        fatal_error(expr->pos, "Implicitly typed compound literals used in context without expected type");
    }
    Type *type = NULL;
    if (expr->compound.type) {
        type = resolve_typespec(expr->compound.type);
    } else {
        type = expected_type;
    }
    complete_type(type);
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
        int index = 0;
        for (size_t i = 0; i < expr->compound.num_fields; i++) {
            CompoundField field = expr->compound.fields[i];
            if (field.kind == FIELD_INDEX) {
                fatal_error(field.pos, "Index field initializer not allowed for struct/union compound literal");
            } else if (field.kind == FIELD_NAME) {
                index = aggregate_field_index(type, field.name);
                if (index == -1)
                {
                    fatal_error(field.pos, "Named field in compound literal does not exist");
                }
            }
            if (index >= type->aggregate.num_fields) {
                fatal_error(field.pos, "Field initializer in struct/union compound literal out of range");
            }
            Type* field_type = type->aggregate.fields[index].type;
            Operand init = resolve_expected_expr(field.init, field_type);
            if (!convert_operand(&init, field_type))
            {
                fatal_error(field.pos, "Illegal conversion in compound literal initializer");
            }
            index++;
        }
    }
    else if (type->kind == TYPE_ARRAY)
    {
        int index = 0, max_index = 0;
        for (size_t i = 0; i < expr->compound.num_fields; i++) {
            CompoundField field = expr->compound.fields[i];
            if (field.kind == FIELD_NAME) {
                fatal_error(field.pos, "Named field initializer not allowed for array compound literals");
            } else if (field.kind == FIELD_INDEX) {
                Operand operand = resolve_const_expr(field.index);
                if (!is_integer_type(operand.type)) {
                    fatal_error(field.pos, "Field initializer index expression must have type int");
                }
                if (!convert_operand(&operand, type_int))
                {
                    fatal_error(field.pos, "Illegal conversion in field initializer index");
                }
                if (operand.val.i < 0) {
                    fatal_error(field.pos, "Field initializer index cannot be negative");
                }
                index = operand.val.i;
            }
            if (type->array.size && index >= type->array.size) {
                fatal_error(field.pos, "Field initializer in array compound literal out of range");
            }
            Operand init = resolve_expected_expr(field.init, type->array.elem);
            if (!convert_operand(&init, type->array.elem)) {
                fatal_error(field.pos, "Illegal conversion in compound literal initializer");
            }
            max_index = MAX(max_index, index);
            index++;
        }
        if (type->array.size == 0) {
            type = type_array(type->array.elem, max_index + 1);
        }
    }
    else
    {
        if (expr->compound.num_fields > 1)
        {
            fatal_error(expr->pos, "Compound literal for scalar type cannot have more than one operand");
        }
        if (expr->compound.num_fields == 1)
        {
            CompoundField field = expr->compound.fields[0];
            Operand init = resolve_expected_expr(field.init, type);
            if (!convert_operand(&init, type))
            {
                fatal_error(field.pos, "Illegal conversion in compound literal initializer");
            }
        }
    }
    return operand_lvalue(type);
}

Operand resolve_expr_call(Expr *expr) {
    assert(expr->kind == EXPR_CALL);
    Operand func = resolve_expr(expr->call.expr);
    if (func.type->kind != TYPE_FUNC) {
        fatal_error(expr->pos, "Trying to call non-function value");
    }
    size_t num_params = func.type->func.num_params;
    if (expr->call.num_args < num_params)
    {
        fatal_error(expr->pos, "Tried to call function with too few arguments");
    }
    if (expr->call.num_args > num_params && !func.type->func.variadic)
    {
        fatal_error(expr->pos, "Tried to call function with too many arguments");
    }
    for (size_t i = 0; i < num_params; i++) {
        Type *param_type = func.type->func.params[i];
        Operand arg = resolve_expected_expr(expr->call.args[i], param_type);
        if (!convert_operand(&arg, param_type))
        {
            fatal_error(expr->call.args[i]->pos, "Illegal conversion in call argument expression");
        }
    }
    for (size_t i = num_params; i < expr->call.num_args; ++i)
    {
        resolve_expr(expr->call.args[i]);
    }
    return operand_rvalue(func.type->func.ret);
}

Operand resolve_expr_ternary(Expr *expr, Type *expected_type) {
    assert(expr->kind == EXPR_TERNARY);
    Operand cond = ptr_decay(resolve_expr(expr->ternary.cond));
    if (!is_scalar_type(cond.type))
    {
        fatal_error(expr->pos, "Ternary conditional must have scalar type");
    }
    Operand left = ptr_decay(resolve_expected_expr(expr->ternary.then_expr, expected_type));
    Operand right = ptr_decay(resolve_expected_expr(expr->ternary.else_expr, expected_type));
    if (is_arithmetic_type(left.type) && is_arithmetic_type(right.type))
    {
        unify_arithmetic_operands(&left, &right);
        if (cond.is_const && left.is_const && right.is_const)
        {
            return operand_const(left.type, cond.val.i ? left.val : right.val);
        }
        else
        {
            return operand_rvalue(left.type);
        }
    }
    else if (left.type == right.type)
    {
        return operand_rvalue(left.type);
    }
    else
    {
        fatal_error(expr->pos, "Left and right operands of ternary expression must have arithmetic types or identical types");
    }
}

Operand resolve_expr_index(Expr *expr) {
    assert(expr->kind == EXPR_INDEX);
    Operand operand = ptr_decay(resolve_expr(expr->index.expr));
    if (operand.type->kind != TYPE_PTR) {
        fatal_error(expr->pos, "Can only index arrays or pointers");
    }
    Operand index = resolve_expr(expr->index.index);
    if (index.type->kind != TYPE_INT) {
        fatal_error(expr->pos, "Index expression must have type int");
    }
    return operand_lvalue(operand.type->ptr.elem);
}

Operand resolve_expr_cast(Expr *expr) {
    assert(expr->kind == EXPR_CAST);
    Type *type = resolve_typespec(expr->cast.type);
    Operand operand = ptr_decay(resolve_expr(expr->cast.expr));
    if (!convert_operand(&operand, type))
    {
        fatal_error(expr->pos, "Illegal conversion in cast");
    }
    return operand;
}

Operand resolve_expected_expr(Expr *expr, Type *expected_type) {
    Operand result;
    switch (expr->kind) {
    case EXPR_INT:
        result = operand_const(type_int, (Val){.i = expr->int_val});
        break;
    case EXPR_FLOAT:
        result = operand_rvalue(type_float);
        break;
    case EXPR_STR:
        result = operand_rvalue(type_ptr(type_char));
        break;
    case EXPR_NAME:
        result = resolve_expr_name(expr);
        break;
    case EXPR_CAST:
        result = resolve_expr_cast(expr);
        break;
    case EXPR_CALL:
        result = resolve_expr_call(expr);
        break;
    case EXPR_INDEX:
        result = resolve_expr_index(expr);
        break;
    case EXPR_FIELD:
        result = resolve_expr_field(expr);
        break;
    case EXPR_COMPOUND:
        result = resolve_expr_compound(expr, expected_type);
        break;
    case EXPR_UNARY:
        result = resolve_expr_unary(expr);
        break;
    case EXPR_BINARY:
        result = resolve_expr_binary(expr);
        break;
    case EXPR_TERNARY:
        result = resolve_expr_ternary(expr, expected_type);
        break;
    case EXPR_SIZEOF_EXPR: {
        Type *type = resolve_expr(expr->sizeof_expr).type;
        complete_type(type);
        result = operand_const(type_usize, (Val){.ull = type_sizeof(type)});
        break;
    }
    case EXPR_SIZEOF_TYPE: {
        Type *type = resolve_typespec(expr->sizeof_type);
        complete_type(type);
        result = operand_const(type_usize, (Val){.ull = type_sizeof(type)});
        break;
    }
    default:
        assert(0);
        result = operand_null;
        break;
    }
    if (result.type) {
        assert(!expr->type || expr->type == result.type);
        expr->type = result.type;
    }
    return result;
}

Operand resolve_expr(Expr *expr) {
    return resolve_expected_expr(expr, NULL);
}

Operand resolve_const_expr(Expr *expr) {
    Operand result = resolve_expr(expr);
    if (!result.is_const) {
        fatal_error(expr->pos, "Expected constant expression");
    }
    return result;
}

void init_global_syms(void) {
    sym_global_type("void", type_void);
    sym_global_type("char", type_char);
    sym_global_type("schar", type_schar);
    sym_global_type("uchar", type_uchar);
    sym_global_type("short", type_short);
    sym_global_type("ushort", type_ushort);
    sym_global_type("int", type_int);
    sym_global_type("uint", type_uint);
    sym_global_type("long", type_long);
    sym_global_type("ulong", type_ulong);
    sym_global_type("llong", type_llong);
    sym_global_type("ullong", type_ullong);
    sym_global_type("float", type_float);
}

void sym_global_decls(DeclSet *declset) {
    for (size_t i = 0; i < declset->num_decls; i++) {
        sym_global_decl(declset->decls[i]);
    }
}

void finalize_syms(void) {
    for (Sym **it = global_syms_buf; it != buf_end(global_syms_buf); it++) {
        Sym *sym = *it;
        if (sym->decl) {
            finalize_sym(sym);
        }
    }
}

void resolve_test(void) {
    assert(promote_type(type_char) == type_int);
    assert(promote_type(type_schar) == type_int);
    assert(promote_type(type_uchar) == type_int);
    assert(promote_type(type_short) == type_int);
    assert(promote_type(type_ushort) == type_int);
    assert(promote_type(type_int) == type_int);
    assert(promote_type(type_uint) == type_uint);
    assert(promote_type(type_long) == type_long);
    assert(promote_type(type_ulong) == type_ulong);
    assert(promote_type(type_llong) == type_llong);
    assert(promote_type(type_ullong) == type_ullong);

    assert(unify_arithmetic_types(type_char, type_char) == type_int);
    assert(unify_arithmetic_types(type_char, type_ushort) == type_int);
    assert(unify_arithmetic_types(type_int, type_uint) == type_uint);
    assert(unify_arithmetic_types(type_int, type_long) == type_long);
    assert(unify_arithmetic_types(type_ulong, type_long) == type_ulong);
    assert(unify_arithmetic_types(type_long, type_uint) == type_ulong);
    assert(unify_arithmetic_types(type_llong, type_ulong) == type_llong);

    assert(convert_val(type_int, type_char, (Val){.c = 100}).i == 100);
    assert(convert_val(type_int, type_char, (Val){.c = -1}).i == -1);
    assert(convert_val(type_uint, type_int, (Val){.i = -1}).u == UINT_MAX);
    assert(convert_val(type_uint, type_ullong, (Val){.ull = ULLONG_MAX}).u == UINT_MAX);

    Type *int_ptr = type_ptr(type_int);
    assert(type_ptr(type_int) == int_ptr);
    Type *float_ptr = type_ptr(type_float);
    assert(type_ptr(type_float) == float_ptr);
    assert(int_ptr != float_ptr);
    Type *int_ptr_ptr = type_ptr(type_ptr(type_int));
    assert(type_ptr(type_ptr(type_int)) == int_ptr_ptr);
    Type *float4_array = type_array(type_float, 4);
    assert(type_array(type_float, 4) == float4_array);
    Type *float3_array = type_array(type_float, 3);
    assert(type_array(type_float, 3) == float3_array);
    assert(float4_array != float3_array);
    Type *int_int_func = type_func(&type_int, 1, type_int, false);
    assert(type_func(&type_int, 1, type_int, false) == int_int_func);
    Type *int_func = type_func(NULL, 0, type_int, false);
    assert(int_int_func != int_func);
    assert(int_func == type_func(NULL, 0, type_int, false));

    init_global_syms();

    const char *code[] = {
        "union IntOrPtr { i: int; p: int*; }",
        "var u1 = IntOrPtr{i = 42}",
        "var u2 = IntOrPtr{p = (:int*)42}",
        "var i: int",
        "struct Vector { x, y: int; }",
        "func f1() { v := Vector{1, 2}; j := i; i++; j++; v.x = 2*j; }",
        "func f2(n: int): int { return 2*n; }",
        "func f3(x: int): int { if (x) { return -x; } else if (x % 2 == 0) { return 42; } else { return -1; } }",
        "func f4(n: int): int { for (i := 0; i < n; i++) { if (i % 3 == 0) { return n; } } return 0; }",
        "func f5(x: int): int { switch(x) { case 0, 1: return 42; case 3: default: return -1; } }",
        "func f6(n: int): int { p := 1; while (n) { p *= 2; n--; } return p; }",
        "func f7(n: int): int { p := 1; do { p *= 2; n--; } while (n); return p; }",
        /*
        "var i: int",
        "func add(v: Vector, w: Vector): Vector { return {v.x + w.x, v.y + w.y}; }",
        "var a: int[256] = {1, 2, ['a'] = 42, [255] = 123}",
        "var v: Vector = 0 ? {1,2} : {3,4}",
        "var vs: Vector[2][2] = {{{1,2},{3,4}}, {{5,6},{7,8}}}",
        "struct A { c: char; }",
        "struct B { i: int; }",
        "struct C { c: char; a: A; }",
        "struct D { c: char; b: B; }",
        "func print(v: Vector) { printf(\"{%d, %d}\", v.x, v.y); }",
        "var x = add({1,2}, {3,4})",
        "var v: Vector = {1,2}",
        "var w = Vector{3,4}",
        "var p: void*",
        "var i = (:int)p + 1",
        "var fp: func(Vector)",
        "struct Dup { x: int; x: int; }",
        "var a: int[3] = {1,2,3}",
        "var b: int[4]",
        "var p = &a[1]",
        "var i = p[1]",
        "var j = *p",
        "const n = sizeof(a)",
        "const m = sizeof(&a[0])",
        "const l = sizeof(1 ? a : b)",
        "var pi = 3.14",
        "var name = \"Per\"",
        "var v = Vector{1,2}",
        "var j = (:int)p",
        "var q = (:int*)j",
        "const i = 42",
        "const j = +i",
        "const k = -i",
        "const a = 1000/((2*3-5) << 1)",
        "const b = !0",
        "const c = ~100 + 1 == -100",
        "const k = 1 ? 2 : 3",
        "union IntOrPtr { i: int; p: int*; }",
        "var i = 42",
        "var u = IntOrPtr{i, &i}",
        "const n = 1+sizeof(p)",
        "var p: T*",
        "var u = *p",
        "struct T { a: int[n]; }",
        "var r = &t.a",
        "var t: T",
        "typedef S = int[n+m]",
        "const m = sizeof(t.a)",
        "var i = n+m",
        "var q = &i",
        "const n = sizeof(x)",
        "var x: T",
        "struct T { s: S*; }",
        "struct S { t: T[n]; }",
*/
    };
    for (size_t i = 0; i < sizeof(code)/sizeof(*code); i++) {
        init_stream(NULL, code[i]);
        Decl *decl = parse_decl();
        sym_global_decl(decl);
    }
    finalize_syms();
    for (Sym **it = sorted_syms; it != buf_end(sorted_syms); it++) {
        Sym *sym = *it;
        if (sym->decl) {
            print_decl(sym->decl);
        } else {
            printf("%s", sym->name);
        }
        printf("\n");
    }
}
