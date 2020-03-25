// This file is in rapid flux, so there's not much point in reporting bugs yet.

char* gen_buf = NULL;

#define genf(...) buf_printf(gen_buf, __VA_ARGS__)
#define genlnf(...) (genln(), genf(__VA_ARGS__))

int gen_indent;

void genln(void)
{
    genf("\n%.*s", gen_indent * 4, "                                                                  ");
}

const char* cdecl_paren(const char* str, bool b)
{
    return b ? strf("(%s)", str) : str;
}

const char* cdecl_name(Type* type)
{
    switch (type->kind)
    {
        case TYPE_VOID:
        {
            return "void";
        } break;

        case TYPE_CHAR:
        {
            return "char";
        } break;

        case TYPE_INT:
        {
            return "int";
        } break;

        case TYPE_FLOAT:
        {
            return "float";
        } break;

        case TYPE_STRUCT:
        case TYPE_UNION:
        {
            return type->sym->name;
        } break;

        default:
        {
            assert(0);
            return NULL;
        } break;
    }
}

char* type_to_cdecl(Type* type, const char* str)
{
    switch (type->kind)
    {
        case TYPE_VOID:
        case TYPE_CHAR:
        case TYPE_INT:
        case TYPE_FLOAT:
        case TYPE_STRUCT:
        case TYPE_UNION:
        {
            return strf("%S%S%S", decl_name(type), *str ? " " : "", str);
        } break;

        case TYPE_PTR:
        {
            return type_to_cdecl(type->ptr.elem, cdecl_paren(strf("*%s", str), *str));
        } break;

        case TYPE_ARRAY:
        {
            return type_to_cdecl(type->array.elem, cdecl_paren(strf("%s[%llu]", str, type->array.size), *str));
        } break;

        case TYPE_FUNC:
        {
            char* result = NULL;
            buf_printf(result, "%s(", cdecl_paren(strf("*%s", str), *str));
            if (type->func.num_params == 0)
            {
                buf_printf(result, "void");
            }
            else
            {
                for (size_t i = 0; i < type->func.num_params; ++i)
                {
                    buf_printf(result, "%s%s", i == 0 ? "" : ", ", type_to_cdecl(type->func.params[i], ""));
                }
            }
            buf_printf(result, ")");
            return type_to_cdecl(type->func.ret, result);
        } break;

        default:
        {
            assert(0);
            return NULL;
        } break;
    }
}

void gen_expr(Expr* expr);

const char* gen_expr_str(Expr* expr)
{
    char* temp = gen_buf;
    gen_buf = NULL;
    gen_expr(expr);
    const char* result = gen_buf;
    gen_buf = temp;
    return result;
}

char* typespec_to_cdecl(Typespec* typespec, const char* str)
{
    // TODO(nicholas): Figure out how to handle type vs typespec in C gen for inferred types. How to prevent "flattened" const values?
    switch (typespec->kind)
    {
        case TYPESPEC_NAME:
        {
            return strf("%S%S%S", typespec->name, *str ? " " : "", str);
        } break;

        case TYPESPEC_PTR:
        {
            return typespec_to_cdecl(typespec->ptr.elem, cdecl_paren(strf("*%s", str), *str));
        } break;

        case TYPESPEC_ARRAY:
        {
            return typespec_to_cdecl(typespec->array.elem, cdecl_paren(strf("%s[%s]", str, gen_expr_str(typespec->array.size)), *str));
        } break;

        case TYPESPEC_FUNC:
        {
            char* result = NULL;
            buf_printf(result, "%s(", cdecl_paren(strf("*%s", str), *str));
            if (typespec->func.num_args == 0)
            {
                buf_printf(result, "void");
            }
            else
            {
                for (size_t i = 0; i < typespec->func.num_args; ++i)
                {
                    buf_printf(result, "%s%s", i == 0 ? "" : ", ", typespec_to_cdecl(typespec->func.args[i], ""));
                }
            }
            buf_printf(result, ")");
            return typespec_to_cdecl(typespec->func.ret, result);
        } break;

        default:
        {
            assert(0);
            return NULL;
        } break;
    }
}

void gen_func_decl(Decl* decl)
{
    assert(decl->kind == DECL_FUNC);
    if (decl->func.ret_type)
    {
        genlnf("%s(", typespec_to_cdecl(decl->func.ret_type, decl->name));
    }
    else
    {
        genlnf("void %s(", decl->name);
    }
    if (decl->func.num_params == 0)
    {
        genf("void");
    }
    else
    {
        for (size_t i = 0; i < decl->func.num_params; ++i)
        {
            FuncParam param = decl->func.params[i];
            if (i != 0)
            {
                genf(", ");
            }
            genf("%s", typespec_to_cdecl(param.type, param.name));
        }
    }
    genf(")");
}

void gen_forward_decls(void)
{
    for (size_t i = 0; i < buf_len(global_syms); ++i)
    {
        Sym* sym = global_syms[i];
        Decl* decl = sym->decl;
        if (!decl)
        {
            continue;
        }
        switch (decl->kind)
        {
            case DECL_STRUCT:
            {
                genlnf("typedef struct %s %s;", sym->name, sym->name);
            } break;

            case DECL_UNION:
            {
                genlnf("typedef union %s %s;", sym->name, sym->name);
            } break;

            case DECL_FUNC:
            {
                gen_func_decl(sym->decl);
                genf(";");
            } break;

            default:
            {
                // Do nothing.
            } break;
        }
    }
}

void gen_aggregate(Decl* decl)
{
    assert(decl->kind == DECL_STRUCT || decl->kind == DECL_UNION);
    genlnf("%s %s {", decl->kind == DECL_STRUCT ? "struct" : "union", decl->name);
    ++gen_indent;
    for (size_t i = 0; i < decl->aggregate.num_items; ++i)
    {
        AggregateItem item = decl->aggregate.items[i];
        if (item.num_names != 1)
        {
            fatal("NYI: only one field allowed per aggregate item decl");
        }
        genlnf("%s;", typespec_to_cdecl(item.type, item.names[0]));
    }
    --gen_indent;
    genlnf("};");
}

void gen_str(const char* str)
{
    // TODO(nicholas): proper quoted string escaping
    genf("\"%s\"", str);
}

void gen_expr(Expr* expr)
{
    switch (expr->kind)
    {
        case EXPR_INT:
        {
            genf("%lld", expr->int_val);
        } break;

        case EXPR_FLOAT:
        {
            genf("%f", expr->float_val);
        } break;

        case EXPR_STR:
        {
            gen_str(expr->str_val);
        } break;

        case EXPR_NAME:
        {
            genf("%s", expr->name);
        } break;

        case EXPR_CAST:
        {
            genf("(%s)(", type_to_cdecl(expr->cast.type->type, ""));
            gen_expr(expr->cast.expr);
            genf(")");
        } break;

        case EXPR_CALL:
        {
            gen_expr(expr->call.expr);
            genf("(");
            for (size_t i = 0; i < expr->call.num_args; ++i)
            {
                if (i != 0)
                {
                    genf(", ");
                }
                gen_expr(expr->call.args[i]);
            }
            genf(")");
        } break;

        case EXPR_INDEX:
        {
            gen_expr(expr->index.expr);
            genf("[");
            gen_expr(expr->index.index);
            genf("]");
        } break;

        case EXPR_FIELD:
        {
            gen_expr(expr->field.expr);
            genf(".%s", expr->field.name);
        } break;

        case EXPR_COMPOUND:
        {
            if (expr->compound.type)
            {
                genf("(%s){", typespec_to_cdecl(expr->compound.type, ""));
            }
            else
            {
                genf("(%s){", type_to_cdecl(expr->type, ""));
            }
            for (size_t i = 0; i < expr->compound.num_fields; ++i)
            {
                if (i != 0)
                {
                    genf(", ");
                }
                CompoundField field = expr->compound.fields[i];
                if (field.kind == FIELD_NAME)
                {
                    genf(".%s = ", field.name);
                }
                else if (field.kind == FIELD_INDEX)
                {
                    genf("[");
                    gen_expr(field.index);
                    genf("]");
                }
                gen_expr(field.init);
            }
            genf("}");
        } break;

        case EXPR_UNARY:
        {
            genf("%s(", token_kind_name(expr->unary.op));
            gen_expr(expr->unary.expr);
            genf(")");
        } break;

        case EXPR_BINARY:
        {
            genf("(");
            gen_expr(expr->binary.left);
            genf(") %s (", token_kind_name(expr->binary.op));
            gen_expr(expr->binary.right);
            genf(")");
        } break;

        case EXPR_TERNARY:
        {
            genf("(");
            gen_expr(expr->ternary.cond);
            genf(" ? ");
            gen_expr(expr->ternary.then_expr);
            genf(" : ");
            gen_expr(expr->ternary.else_expr);
            genf(")");
        } break;

        case EXPR_SIZEOF_EXPR:
        {
            genf("sizeof(");
            gen_expr(expr->sizeof_expr);
            genf(")");
        } break;

        case EXPR_SIZEOF_TYPE:
        {
            genf("sizeof(%s)", type_to_cdecl(expr->sizeof_type->type, ""));
        } break;

        default:
        {
            assert(0);
        } break;
    }
}

// TODO - start back here, day 10