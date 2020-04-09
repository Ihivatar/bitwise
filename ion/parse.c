Decl *parse_decl_opt(void);
Decl *parse_decl(void);
Typespec *parse_type(void);
Stmt *parse_stmt(void);
Expr *parse_expr(void);

Typespec *parse_type_func(void) {
    SrcPos pos = token.pos;
    Typespec **args = NULL;
    bool variadic = false;
    expect_token(TOKEN_LPAREN);
    if (!is_token(TOKEN_RPAREN)) {
        buf_push(args, parse_type());
        while (match_token(TOKEN_COMMA))
        {
            if (match_token(TOKEN_ELLIPSES))
            {
                if (variadic)
                {
                    syntax_error("Multiple ellipsis instances in function type");
                }
                variadic = true;
            }
            else
            {
                if (variadic)
                {
                    syntax_error("Ellipsis must be last parameter in function type");
                }
                buf_push(args, parse_type());
            }
        }
    }
    expect_token(TOKEN_RPAREN);
    Typespec *ret = NULL;
    if (match_token(TOKEN_COLON)) {
        ret = parse_type();
    }
    return typespec_func(pos, args, buf_len(args), ret, variadic);
}

Typespec *parse_type_base(void) {
    if (is_token(TOKEN_NAME)) {
        SrcPos pos = token.pos;
        const char *name = token.name;
        next_token();
        return typespec_name(pos, name);
    } else if (match_keyword(func_keyword)) {
        return parse_type_func();
    } else if (match_token(TOKEN_LPAREN)) {
        Typespec *type = parse_type();
        expect_token(TOKEN_RPAREN);
        return type;
    } else {
        fatal_syntax_error("Unexpected token %s in type", token_info());
        return NULL;
    }
}

Typespec *parse_type(void) {
    Typespec *type = parse_type_base();
    SrcPos pos = token.pos;
    while (is_token(TOKEN_LBRACKET) || is_token(TOKEN_MUL)) {
        if (match_token(TOKEN_LBRACKET)) {
            Expr *size = NULL;
            if (!is_token(TOKEN_RBRACKET)) {
                size = parse_expr();
            }
            expect_token(TOKEN_RBRACKET);
            type = typespec_array(pos, type, size);
        } else {
            assert(is_token(TOKEN_MUL));
            next_token();
            type = typespec_ptr(pos, type);
        }
    }
    return type;
}

CompoundField parse_expr_compound_field(void) {
    SrcPos pos = token.pos;
    if (match_token(TOKEN_LBRACKET)) {
        Expr *index = parse_expr();
        expect_token(TOKEN_RBRACKET);
        expect_token(TOKEN_ASSIGN);
        return (CompoundField){FIELD_INDEX, pos, parse_expr(), .index = index};
    } else {
        Expr *expr = parse_expr();
        if (match_token(TOKEN_ASSIGN)) {
            if (expr->kind != EXPR_NAME) {
                fatal_syntax_error("Named initializer in compound literal must be preceded by field name");
            }
            return (CompoundField){FIELD_NAME, pos, parse_expr(), .name = expr->name};
        } else {
            return (CompoundField){FIELD_DEFAULT, pos, expr};
        }
    }
}

Expr *parse_expr_compound(Typespec *type) {
    SrcPos pos = token.pos;
    expect_token(TOKEN_LBRACE);
    CompoundField *fields = NULL;
    while (!is_token(TOKEN_RBRACE)) {
        buf_push(fields, parse_expr_compound_field());
        if (!match_token(TOKEN_COMMA)) {
            break;
        }
    }
    expect_token(TOKEN_RBRACE);
    return expr_compound(pos, type, fields, buf_len(fields));
}

Expr *parse_expr_unary(void);

Expr *parse_expr_operand(void) {
    SrcPos pos = token.pos;
    if (is_token(TOKEN_INT)) {
        int val = token.int_val;
        next_token();
        return expr_int(pos, val);
    } else if (is_token(TOKEN_FLOAT)) {
        double val = token.float_val;
        next_token();
        return expr_float(pos, val);
    } else if (is_token(TOKEN_STR)) {
        const char *val = token.str_val;
        next_token();
        return expr_str(pos, val);
    } else if (is_token(TOKEN_NAME)) {
        const char *name = token.name;
        next_token();
        if (is_token(TOKEN_LBRACE)) {
            return parse_expr_compound(typespec_name(pos, name));
        } else {
            return expr_name(pos, name);
        }
    } else if (match_keyword(sizeof_keyword)) {
        expect_token(TOKEN_LPAREN);
        if (match_token(TOKEN_COLON)) {
            Typespec *type = parse_type();
            expect_token(TOKEN_RPAREN);
            return expr_sizeof_type(pos, type);
        } else {
            Expr *expr = parse_expr();
            expect_token(TOKEN_RPAREN);
            return expr_sizeof_expr(pos, expr);
        }
    } else if (is_token(TOKEN_LBRACE)) {
        return parse_expr_compound(NULL);
    } else if (match_token(TOKEN_LPAREN)) {
        if (match_token(TOKEN_COLON)) {
            Typespec *type = parse_type();
            expect_token(TOKEN_RPAREN);
            if (is_token(TOKEN_LBRACE)) {
                return parse_expr_compound(type);
            } else {
                return expr_cast(pos, type, parse_expr_unary());
            }
        } else {
            Expr *expr = parse_expr();
            expect_token(TOKEN_RPAREN);
            return expr;
        }
    } else {
        fatal_syntax_error("Unexpected token %s in expression", token_info());
        return NULL;
    }
}

Expr *parse_expr_base(void) {
    Expr *expr = parse_expr_operand();
    while (is_token(TOKEN_LPAREN) || is_token(TOKEN_LBRACKET) || is_token(TOKEN_DOT)) {
        SrcPos pos = token.pos;
        if (match_token(TOKEN_LPAREN)) {
            Expr **args = NULL;
            if (!is_token(TOKEN_RPAREN)) {
                buf_push(args, parse_expr());
                while (match_token(TOKEN_COMMA)) {
                    buf_push(args, parse_expr());
                }
            }
            expect_token(TOKEN_RPAREN);
            expr = expr_call(pos, expr, args, buf_len(args));
        } else if (match_token(TOKEN_LBRACKET)) {
            Expr *index = parse_expr();
            expect_token(TOKEN_RBRACKET);
            expr = expr_index(pos, expr, index);
        } else {
            assert(is_token(TOKEN_DOT));
            next_token();
            const char *field = token.name;
            expect_token(TOKEN_NAME);
            expr = expr_field(pos, expr, field);
        }
    }
    return expr;
}

bool is_unary_op(void) {
    return
        is_token(TOKEN_ADD) ||
        is_token(TOKEN_SUB) ||
        is_token(TOKEN_MUL) ||
        is_token(TOKEN_AND) ||
        is_token(TOKEN_NEG) ||
        is_token(TOKEN_NOT);
}

Expr *parse_expr_unary(void) {
    if (is_unary_op()) {
        SrcPos pos = token.pos;
        TokenKind op = token.kind;
        next_token();
        return expr_unary(pos, op, parse_expr_unary());
    } else {
        return parse_expr_base();
    }
}

bool is_mul_op(void) {
    return TOKEN_FIRST_MUL <= token.kind && token.kind <= TOKEN_LAST_MUL;
}

Expr *parse_expr_mul(void) {
    Expr *expr = parse_expr_unary();
    while (is_mul_op()) {
        SrcPos pos = token.pos;
        TokenKind op = token.kind;
        next_token();
        expr = expr_binary(pos, op, expr, parse_expr_unary());
    }
    return expr;
}

bool is_add_op(void) {
    return TOKEN_FIRST_ADD <= token.kind && token.kind <= TOKEN_LAST_ADD;
}

Expr *parse_expr_add(void) {
    Expr *expr = parse_expr_mul();
    while (is_add_op()) {
        SrcPos pos = token.pos;
        TokenKind op = token.kind;
        next_token();
        expr = expr_binary(pos, op, expr, parse_expr_mul());
    }
    return expr;
}

bool is_cmp_op(void) {
    return TOKEN_FIRST_CMP <= token.kind && token.kind <= TOKEN_LAST_CMP;
}

Expr *parse_expr_cmp(void) {
    Expr *expr = parse_expr_add();
    while (is_cmp_op()) {
        SrcPos pos = token.pos;
        TokenKind op = token.kind;
        next_token();
        expr = expr_binary(pos, op, expr, parse_expr_add());
    }
    return expr;
}

Expr *parse_expr_and(void) {
    Expr *expr = parse_expr_cmp();
    while (match_token(TOKEN_AND_AND)) {
        SrcPos pos = token.pos;
        expr = expr_binary(pos, TOKEN_AND_AND, expr, parse_expr_cmp());
    }
    return expr;
}

Expr *parse_expr_or(void) {
    Expr *expr = parse_expr_and();
    while (match_token(TOKEN_OR_OR)) {
        SrcPos pos = token.pos;
        expr = expr_binary(pos, TOKEN_OR_OR, expr, parse_expr_and());
    }
    return expr;
}

Expr *parse_expr_ternary(void) {
    SrcPos pos = token.pos;
    Expr *expr = parse_expr_or();
    if (match_token(TOKEN_QUESTION)) {
        Expr *then_expr = parse_expr_ternary();
        expect_token(TOKEN_COLON);
        Expr *else_expr = parse_expr_ternary();
        expr = expr_ternary(pos, expr, then_expr, else_expr);
    }
    return expr;
}

Expr *parse_expr(void) {
    return parse_expr_ternary();
}

Expr *parse_paren_expr(void) {
    expect_token(TOKEN_LPAREN);
    Expr *expr = parse_expr();
    expect_token(TOKEN_RPAREN);
    return expr;
}

StmtList parse_stmt_block(void) {
    SrcPos pos = token.pos;
    expect_token(TOKEN_LBRACE);
    Stmt **stmts = NULL;
    while (!is_token_eof() && !is_token(TOKEN_RBRACE)) {
        buf_push(stmts, parse_stmt());
    }
    expect_token(TOKEN_RBRACE);
    return stmt_list(pos, stmts, buf_len(stmts));
}

Stmt *parse_stmt_if(SrcPos pos) {
    Expr *cond = parse_paren_expr();
    StmtList then_block = parse_stmt_block();
    StmtList else_block = {0};
    ElseIf *elseifs = NULL;
    while (match_keyword(else_keyword)) {
        if (!match_keyword(if_keyword)) {
            else_block = parse_stmt_block();
            break;
        }
        Expr *elseif_cond = parse_paren_expr();
        StmtList elseif_block = parse_stmt_block();
        buf_push(elseifs, (ElseIf){elseif_cond, elseif_block});
    }
    return stmt_if(pos, cond, then_block, elseifs, buf_len(elseifs), else_block);
}

Stmt *parse_stmt_while(SrcPos pos) {
    Expr *cond = parse_paren_expr();
    return stmt_while(pos, cond, parse_stmt_block());
}

Stmt *parse_stmt_do_while(SrcPos pos) {
    StmtList block = parse_stmt_block();
    if (!match_keyword(while_keyword)) {
        fatal_syntax_error("Expected 'while' after 'do' block");
        return NULL;
    }
    Stmt *stmt = stmt_do_while(pos, parse_paren_expr(), block);
    expect_token(TOKEN_SEMICOLON);
    return stmt;
}

bool is_assign_op(void) {
    return TOKEN_FIRST_ASSIGN <= token.kind && token.kind <= TOKEN_LAST_ASSIGN;
}

Stmt *parse_simple_stmt(void) {
    Expr *expr = parse_expr();
    Stmt *stmt;
    if (match_token(TOKEN_COLON_ASSIGN)) {
        if (expr->kind != EXPR_NAME) {
            fatal_syntax_error(":= must be preceded by a name");
            return NULL;
        }
        stmt = stmt_init(expr->pos, expr->name, parse_expr());
    } else if (is_assign_op()) {
        TokenKind op = token.kind;
        next_token();
        stmt = stmt_assign(expr->pos, op, expr, parse_expr());
    } else if (is_token(TOKEN_INC) || is_token(TOKEN_DEC)) {
        TokenKind op = token.kind;
        next_token();
        stmt = stmt_assign(expr->pos, op, expr, NULL);
    } else {
        stmt = stmt_expr(expr->pos, expr);
    }
    return stmt;
}

Stmt *parse_stmt_for(SrcPos pos) {
    expect_token(TOKEN_LPAREN);
    Stmt *init = NULL;
    if (!is_token(TOKEN_SEMICOLON)) {
        init = parse_simple_stmt();
    }
    expect_token(TOKEN_SEMICOLON);
    Expr *cond = NULL;
    if (!is_token(TOKEN_SEMICOLON)) {
        cond = parse_expr();
    }
    expect_token(TOKEN_SEMICOLON);
    Stmt *next = NULL;
    if (!is_token(TOKEN_RPAREN)) {
        next = parse_simple_stmt();
        if (next->kind == STMT_INIT) {
            syntax_error("Init statements not allowed in for-statement's next clause");
        }
    }
    expect_token(TOKEN_RPAREN);
    return stmt_for(pos, init, cond, next, parse_stmt_block());
}

SwitchCase parse_stmt_switch_case(void) {
    Expr **exprs = NULL;
    bool is_default = false;
    while (is_keyword(case_keyword) || is_keyword(default_keyword)) {
        if (match_keyword(case_keyword)) {
            buf_push(exprs, parse_expr());
            while (match_token(TOKEN_COMMA)) {
                buf_push(exprs, parse_expr());
            }
        } else {
            assert(is_keyword(default_keyword));
            next_token();
            if (is_default) {
                syntax_error("Duplicate default labels in same switch clause");
            }
            is_default = true;
        }
        expect_token(TOKEN_COLON);
    }
    SrcPos pos = token.pos;
    Stmt **stmts = NULL;
    while (!is_token_eof() && !is_token(TOKEN_RBRACE) && !is_keyword(case_keyword) && !is_keyword(default_keyword)) {
        buf_push(stmts, parse_stmt());
    }
    return (SwitchCase){exprs, buf_len(exprs), is_default, stmt_list(pos, stmts, buf_len(stmts))};
}

Stmt *parse_stmt_switch(SrcPos pos) {
    Expr *expr = parse_paren_expr();
    SwitchCase *cases = NULL;
    expect_token(TOKEN_LBRACE);
    while (!is_token_eof() && !is_token(TOKEN_RBRACE)) {
        buf_push(cases, parse_stmt_switch_case());
    }
    expect_token(TOKEN_RBRACE);
    return stmt_switch(pos, expr, cases, buf_len(cases));
}

Stmt *parse_stmt(void) {
    SrcPos pos = token.pos;
    if (match_keyword(if_keyword)) {
        return parse_stmt_if(pos);
    } else if (match_keyword(while_keyword)) {
        return parse_stmt_while(pos);
    } else if (match_keyword(do_keyword)) {
        return parse_stmt_do_while(pos);
    } else if (match_keyword(for_keyword)) {
        return parse_stmt_for(pos);
    } else if (match_keyword(switch_keyword)) {
        return parse_stmt_switch(pos);
    } else if (is_token(TOKEN_LBRACE)) {
        return stmt_block(pos, parse_stmt_block());
    } else if (match_keyword(break_keyword)) {
        expect_token(TOKEN_SEMICOLON);
        return stmt_break(pos);
    } else if (match_keyword(continue_keyword)) {
        expect_token(TOKEN_SEMICOLON);
        return stmt_continue(pos);
    } else if (match_keyword(return_keyword)) {
        Expr *expr = NULL;
        if (!is_token(TOKEN_SEMICOLON)) {
            expr = parse_expr();
        }
        expect_token(TOKEN_SEMICOLON);
        return stmt_return(pos, expr);
    } else {
        Decl *decl = parse_decl_opt();
        if (decl) {
            return stmt_decl(pos, decl);
        }
        Stmt *stmt = parse_simple_stmt();
        expect_token(TOKEN_SEMICOLON);
        return stmt;
    }
}

const char *parse_name(void) {
    const char *name = token.name;
    expect_token(TOKEN_NAME);
    return name;
}

EnumItem parse_decl_enum_item(void) {
    SrcPos pos = token.pos;
    const char *name = parse_name();
    Expr *init = NULL;
    if (match_token(TOKEN_ASSIGN)) {
        init = parse_expr();
    }
    return (EnumItem){pos, name, init};
}

Decl *parse_decl_enum(SrcPos pos) {
    const char *name = parse_name();
    expect_token(TOKEN_LBRACE);
    EnumItem *items = NULL;
    while (!is_token(TOKEN_RBRACE)) {
        buf_push(items, parse_decl_enum_item());
        if (!match_token(TOKEN_COMMA)) {
            break;
        }
    }
    expect_token(TOKEN_RBRACE);
    return decl_enum(pos, name, items, buf_len(items));
}

AggregateItem parse_decl_aggregate_item(void) {
    SrcPos pos = token.pos;
    const char **names = NULL;
    buf_push(names, parse_name());
    while (match_token(TOKEN_COMMA)) {
        buf_push(names, parse_name());
    }
    expect_token(TOKEN_COLON);
    Typespec *type = parse_type();
    expect_token(TOKEN_SEMICOLON);
    return (AggregateItem){pos, names, buf_len(names), type};
}

Decl *parse_decl_aggregate(SrcPos pos, DeclKind kind) {
    assert(kind == DECL_STRUCT || kind == DECL_UNION);
    const char *name = parse_name();
    expect_token(TOKEN_LBRACE);
    AggregateItem *items = NULL;
    while (!is_token_eof() && !is_token(TOKEN_RBRACE)) {
        buf_push(items, parse_decl_aggregate_item());
    }
    expect_token(TOKEN_RBRACE);
    return decl_aggregate(pos, kind, name, items, buf_len(items));
}

Decl *parse_decl_var(SrcPos pos) {
    const char *name = parse_name();
    if (match_token(TOKEN_ASSIGN)) {
        return decl_var(pos, name, NULL, parse_expr());
    } else if (match_token(TOKEN_COLON)) {
        Typespec *type = parse_type();
        Expr *expr = NULL;
        if (match_token(TOKEN_ASSIGN)) {
            expr = parse_expr();
        }
        return decl_var(pos, name, type, expr);
    } else {
        fatal_syntax_error("Expected : or = after var, got %s", token_info());
        return NULL;
    }
}

Decl *parse_decl_const(SrcPos pos) {
    const char *name = parse_name();
    expect_token(TOKEN_ASSIGN);
    return decl_const(pos, name, parse_expr());
}

Decl *parse_decl_typedef(SrcPos pos) {
    const char *name = parse_name();
    expect_token(TOKEN_ASSIGN);
    return decl_typedef(pos, name, parse_type());
}

FuncParam parse_decl_func_param(void) {
    SrcPos pos = token.pos;
    const char *name = parse_name();
    expect_token(TOKEN_COLON);
    Typespec *type = parse_type();
    return (FuncParam){pos, name, type};
}

Decl *parse_decl_func(SrcPos pos) {
    const char *name = parse_name();
    expect_token(TOKEN_LPAREN);
    FuncParam *params = NULL;
    bool variadic = false;
    if (!is_token(TOKEN_RPAREN)) {
        buf_push(params, parse_decl_func_param());
        while (match_token(TOKEN_COMMA))
        {
            if (match_token(TOKEN_ELLIPSES))
            {
                if (variadic)
                {
                    syntax_error("Multiple ellipsis in function declaration");
                }
                variadic = true;
            }
            else
            {
                if (variadic)
                {
                    syntax_error("Ellipsis must be last parameter in function declaration");
                }
                buf_push(params, parse_decl_func_param());
            }
        }
    }
    expect_token(TOKEN_RPAREN);
    Typespec *ret_type = NULL;
    if (match_token(TOKEN_COLON)) {
        ret_type = parse_type();
    }
    StmtList block = parse_stmt_block();
    return decl_func(pos, name, params, buf_len(params), ret_type, variadic, block);
}

NoteList parse_note_list(void)
{
    Note* notes = NULL;
    while (match_token(TOKEN_AT))
    {
        buf_push(notes, (Note){.pos = token.pos, .name = parse_name()});
    }
    return note_list(notes, buf_len(notes));
}

Decl *parse_decl_opt(void) {
    SrcPos pos = token.pos;
    if (match_keyword(enum_keyword)) {
        return parse_decl_enum(pos);
    } else if (match_keyword(struct_keyword)) {
        return parse_decl_aggregate(pos, DECL_STRUCT);
    } else if (match_keyword(union_keyword)) {
        return parse_decl_aggregate(pos, DECL_UNION);
    } else if (match_keyword(var_keyword)) {
        return parse_decl_var(pos);
    } else if (match_keyword(const_keyword)) {
        return parse_decl_const(pos);
    } else if (match_keyword(typedef_keyword)) {
        return parse_decl_typedef(pos);
    } else if (match_keyword(func_keyword)) {
        return parse_decl_func(pos);
    } else {
        return NULL;
    }
}

Decl *parse_decl(void) {
    NoteList notes = parse_note_list();
    Decl *decl = parse_decl_opt();
    if (!decl) {
        fatal_syntax_error("Expected declaration keyword, got %s", token_info());
    }
    decl->notes = notes;
    return decl;
}

DeclSet *parse_file(void) {
    Decl **decls = NULL;
    while (!is_token(TOKEN_EOF)) {
        Decl *decl = parse_decl();
        assert(decl);
        buf_push(decls, decl);
    }
    return decl_set(decls, buf_len(decls));
}

void parse_test(void) {
    const char *decls[] = {
        "var x: char[256] = {1, 2, 3, ['a'] = 4}",
        "struct Vector { x, y: float; }",
        "var v = Vector{x = 1.0, y = -1.0}",
        "var v: Vector = {1.0, -1.0}",
        "const n = sizeof(:int*[16])",
        "const n = sizeof(1+2)",
        "var x = b == 1 ? 1+2 : 3-4",
        "func fact(n: int): int { trace(\"fact\"); if (n == 0) { return 1; } else { return n * fact(n-1); } }",
        "func fact(n: int): int { p := 1; for (i := 1; i <= n; i++) { p *= i; } return p; }",
        "var foo = a ? a&b + c<<d + e*f == +u-v-w + *g/h(x,y) + -i%k[x] && m <= n*(p+q)/r : 0",
        "func f(x: int): bool { switch (x) { case 0: case 1: return true; case 2: default: return false; } }",
        "enum Color { RED = 3, GREEN, BLUE = 0 }",
        "const pi = 3.14",
        "union IntOrFloat { i: int; f: float; }",
        "typedef Vectors = Vector[1+2]",
        "func f() { do { print(42); } while(1); }",
        "typedef T = (func(int):int)[16]",
        "func f() { enum E { A, B, C } return; }",
        "func f() { if (1) { return 1; } else if (2) { return 2; } else { return 3; } }",
    };
    for (const char **it = decls; it != decls + sizeof(decls)/sizeof(*decls); it++) {
        init_stream(NULL, *it);
        Decl *decl = parse_decl();
        print_decl(decl);
        printf("\n");
    }
}
