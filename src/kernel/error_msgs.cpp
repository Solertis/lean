/*
Copyright (c) 2014 Microsoft Corporation. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: Leonardo de Moura
*/
#include <string>
#include "kernel/replace_fn.h"
#include "kernel/error_msgs.h"

namespace lean {
format pp_indent_expr(formatter const & fmt, expr const & e) {
    return nest(get_pp_indent(fmt.get_options()), compose(line(), fmt(e)));
}

format pp_type_expected(formatter const & fmt, expr const & e) {
    return compose(format("type expected at"), pp_indent_expr(fmt, e));
}

format pp_function_expected(formatter const & fmt, expr const & e) {
    return compose(format("function expected at"), pp_indent_expr(fmt, e));
}

static list<options> * g_distinguishing_pp_options = nullptr;

void set_distinguishing_pp_options(list<options> const & opts) {
    *g_distinguishing_pp_options = opts;
}

list<options> const & get_distinguishing_pp_options() {
    return *g_distinguishing_pp_options;
}

expr erase_binder_info(expr const & e) {
    return replace(e, [](expr const & e) {
            if (is_local(e) || is_metavar(e)) {
                return some_expr(e);
            } else if (is_binding(e) && binding_info(e) != binder_info()) {
                return some_expr(update_binding(e, erase_binder_info(binding_domain(e)),
                                                erase_binder_info(binding_body(e)), binder_info()));
            } else {
                return none_expr();
            }
        });
}

static std::tuple<format, format> pp_until_different(formatter const & fmt, expr const & e1, expr const & e2, list<options> extra) {
    formatter fmt1 = fmt;
    expr n_e1 = erase_binder_info(e1);
    expr n_e2 = erase_binder_info(e2);
    while (true) {
        format r1 = pp_indent_expr(fmt1, n_e1);
        format r2 = pp_indent_expr(fmt1, n_e2);
        if (!format_pp_eq(r1, r2, fmt1.get_options()))
            return mk_pair(pp_indent_expr(fmt1, e1), pp_indent_expr(fmt1, e2));
        if (!extra)
            return mk_pair(pp_indent_expr(fmt, e1), pp_indent_expr(fmt, e2));
        options o = join(head(extra), fmt.get_options());
        fmt1  = fmt.update_options(o);
        extra = tail(extra);
    }
}

std::tuple<format, format> pp_until_different(formatter const & fmt, expr const & e1, expr const & e2) {
    return pp_until_different(fmt, e1, e2, *g_distinguishing_pp_options);
}

format pp_app_type_mismatch(formatter const & _fmt, expr const & app, expr const & fn_type, expr const & arg, expr const & given_type, bool as_error) {
    formatter fmt(_fmt);
    format r;
    format expected_fmt, given_fmt;
    lean_assert(is_pi(fn_type));
    if (!is_explicit(binding_info(fn_type))) {
        // For implicit arguments to be shown if argument is implicit
        options opts = fmt.get_options();
        // TODO(Leo): this is hackish, the option is defined in the folder library
        opts = opts.update_if_undef(name{"pp", "implicit"}, true);
        fmt = fmt.update_options(opts);
    }
    if (is_lambda(get_app_fn(app))) {
        // Disable beta reduction in the pretty printer since it will make the error hard to understand.
        // See issue https://github.com/leanprover/lean/issues/669
        options opts = fmt.get_options();
        // TODO(Leo): this is hackish, the option is defined in the folder library
        opts = opts.update_if_undef(name{"pp", "beta"}, false);
        fmt = fmt.update_options(opts);
    }
    expr expected_type = binding_domain(fn_type);
    std::tie(expected_fmt, given_fmt) = pp_until_different(fmt, expected_type, given_type);
    if (as_error)
        r += format("type mismatch at application");
    else
        r += format("application type constraint");
    r += pp_indent_expr(fmt, app);
    r += compose(line(), format("term"));
    r += pp_indent_expr(fmt, arg);
    r += compose(line(), format("has type"));
    r += given_fmt;
    if (as_error) {
        r += compose(line(), format("but is expected to have type"));
        r += expected_fmt;
    }
    return r;
}

format pp_def_type_mismatch(formatter const & fmt, name const & n, expr const & expected_type, expr const & given_type, bool as_error) {
    format expected_fmt, given_fmt;
    std::tie(expected_fmt, given_fmt) = pp_until_different(fmt, expected_type, given_type);
    format r;
    if (as_error)
        r += format("type mismatch at definition '");
    else
        r += format("definition type constraint '");
    r += format(n);
    r += format("', has type");
    r += given_fmt;
    if (as_error) {
        r += compose(line(), format("but is expected to have type"));
        r += expected_fmt;
    }
    return r;
}

static format pp_until_meta_visible(formatter const & fmt, expr const & e, list<options> extra) {
    options o = fmt.get_options();
    o = o.update_if_undef(get_formatter_hide_full_terms_name(), true);
    formatter fmt1 = fmt.update_options(o);
    while (true) {
        format r = pp_indent_expr(fmt1, e);
        std::ostringstream out;
        out << mk_pair(r, fmt1.get_options());
        if (out.str().find("?M") != std::string::npos)
            return r;
        if (!extra)
            return pp_indent_expr(fmt.update_options(o), e);
        options o2 = join(head(extra), fmt.get_options());
        o2    = o2.update_if_undef(get_formatter_hide_full_terms_name(), true);
        fmt1  = fmt.update_options(o2);
        extra = tail(extra);
    }
}

format pp_until_meta_visible(formatter const & fmt, expr const & e) {
    return pp_until_meta_visible(fmt, e, *g_distinguishing_pp_options);
}

format pp_decl_has_metavars(formatter const & fmt, name const & n, expr const & e, bool is_type) {
    format r("failed to add declaration '");
    r += format(n);
    r += format("' to environment, ");
    if (is_type)
        r += format("type");
    else
        r += format("value");
    r += format(" has metavariables");
    options const & o = fmt.get_options();
    if (!o.contains(get_formatter_hide_full_terms_name()))
        r += line() + format("remark: set 'formatter.hide_full_terms' to false to see the complete term");
    r += pp_until_meta_visible(fmt, e);
    return r;
}

void initialize_error_msgs() {
    g_distinguishing_pp_options = new list<options>();
}
void finalize_error_msgs() {
    delete g_distinguishing_pp_options;
}
}
