import argparse
import json
import sys
from dataclasses import dataclass
from typing import Optional
from pathlib import Path

import clang.cindex as cl
import pefile

DIR = Path(__file__).parent.resolve()
INCLUDE_DIR = DIR / 'include'
EXPORTS_JSON = DIR / 'exe_exports.json'

IGNORED_TYPES = {
    cl.CursorKind.TYPEDEF_DECL,
    cl.CursorKind.STRUCT_DECL,
    cl.CursorKind.CLASS_DECL,
    cl.CursorKind.ENUM_DECL,
    cl.CursorKind.UNION_DECL,
    cl.CursorKind.UNEXPOSED_DECL,  # Hope these aren't important
    cl.CursorKind.VAR_DECL,
    # Templates are inlined
    cl.CursorKind.FUNCTION_TEMPLATE,
    cl.CursorKind.CLASS_TEMPLATE,
    cl.CursorKind.TYPE_ALIAS_DECL,
    cl.CursorKind.TYPE_ALIAS_TEMPLATE_DECL,
    cl.CursorKind.USING_DECLARATION,
    cl.CursorKind.STATIC_ASSERT,
    cl.CursorKind.USING_DIRECTIVE,  # Not used in the WWise headers, but useful for testing

    # We may need to handle these later
    cl.CursorKind.CONSTRUCTOR,
    cl.CursorKind.CXX_METHOD,
    cl.CursorKind.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION,
}

BAD_IDENT_CHARS = {
    '*',
    '(', ')',
    '[', ']',
}

BASE_TYPE_KINDS = {
    cl.TypeKind.INVALID,
    cl.TypeKind.UNEXPOSED,
    cl.TypeKind.VOID,
    cl.TypeKind.BOOL,
    cl.TypeKind.CHAR_U,
    cl.TypeKind.UCHAR,
    cl.TypeKind.CHAR16,
    cl.TypeKind.CHAR32,
    cl.TypeKind.USHORT,
    cl.TypeKind.UINT,
    cl.TypeKind.ULONG,
    cl.TypeKind.ULONGLONG,
    cl.TypeKind.UINT128,
    cl.TypeKind.CHAR_S,
    cl.TypeKind.SCHAR,
    cl.TypeKind.WCHAR,
    cl.TypeKind.SHORT,
    cl.TypeKind.INT,
    cl.TypeKind.LONG,
    cl.TypeKind.LONGLONG,
    cl.TypeKind.INT128,
    cl.TypeKind.FLOAT,
    cl.TypeKind.DOUBLE,
    cl.TypeKind.LONGDOUBLE,
    cl.TypeKind.NULLPTR,
}


@dataclass
class Parameter:
    name: Optional[str]
    type: str


@dataclass
class Function:
    name: str
    args: list[Parameter]
    ret: str
    mangled_name: str
    namespace: str
    idx: int

    def identifier_safe_name(self):
        name = self.name.replace(' ', '_')

        if BAD_IDENT_CHARS.intersection(name):
            for c in BAD_IDENT_CHARS:
                name = name.replace(c, '_')

        return name

    def var_name(self):
        return f"ptr{self.idx:03d}_{self.identifier_safe_name()}"


class SearchInfo:
    functions: list[Function]
    required_headers: set[Path]
    skip_messages = ""

    def __init__(self):
        self.functions = []
        self.required_headers = set()


def resolve_type(t: cl.Type, info: SearchInfo):
    # print("==")
    # print(t.spelling)
    # print(t.get_declaration().location)
    # print(t.kind)

    # The basic types - these are all already fully-qualified
    if t.kind in BASE_TYPE_KINDS:
        return t.spelling

    prefix = ""

    if t.is_const_qualified():
        prefix += "const "
    if t.is_volatile_qualified():
        prefix += "volatile "
    if t.is_restrict_qualified():
        prefix += "restrict "

    match t.kind:
        case cl.TypeKind.ELABORATED:
            # Get the declaration this type refers to
            named_type = t.get_named_type()

            # Keep track of all the headers we need
            # file is none for AK::AkDeviceStatusCallbackFunc?
            loc = named_type.get_declaration().location
            if loc.file is not None:
                path = Path(loc.file.name)
                info.required_headers.add(path)

            # This gets the fully-qualified name
            return prefix + named_type.spelling
        case cl.TypeKind.POINTER:
            pointee = resolve_type(t.get_pointee(), info)
            pointer = pointee + "*"
            return prefix + pointer
        case cl.TypeKind.LVALUEREFERENCE:
            pointee = resolve_type(t.get_pointee(), info)
            pointer = pointee + "&"
            return prefix + pointer
        case _:
            raise Exception(f'Unhandled type {t.kind}')

    return t.spelling


def search(cursor: cl.Cursor, namespace, info: SearchInfo):
    for child in cursor.get_children():
        child: cl.Cursor

        # Ignore Windows SDK files etc
        loc: cl.SourceLocation = child.location
        path = Path(loc.file.name)
        if DIR not in path.parents:
            continue

        if child.kind == cl.CursorKind.LINKAGE_SPEC:
            search(child, namespace, info)
        elif child.kind == cl.CursorKind.NAMESPACE:
            if namespace == '':
                child_prefix = child.spelling
            else:
                child_prefix = namespace + '::' + child.spelling
            # print(child_prefix)
            search(child, child_prefix, info)
        elif child.kind in IGNORED_TYPES:
            pass
        elif child.kind == cl.CursorKind.FUNCTION_DECL:
            # If this function has a definition in the headers, we'll just use that.
            # (and hope it matches exactly what's in the binary :3 )
            if child.get_definition() is not None:
                continue

            # For now, don't bother with variadic functions
            if child.type.is_function_variadic():
                msg = f"Skipping variadic function {child.spelling}"
                print(msg)
                info.skip_messages += f"// {msg}\n"
                continue

            params = []
            for arg in child.get_arguments():
                name = arg.displayname
                arg_type = resolve_type(arg.type, info)
                params.append(Parameter(None if name == '' else name, arg_type))

            # print(child.spelling)
            method = Function(
                name=child.spelling,
                args=params,
                ret=resolve_type(child.result_type, info),
                mangled_name=child.mangled_name,
                namespace=namespace,
                idx=len(info.functions),
            )
            # print(method)
            # print(method.ret)
            info.functions.append(method)
        else:
            print(loc)
            print(child.kind)
            print(child.spelling)
            raise Exception(f'Unhandled cursor kind {child.kind}')
            sys.exit(1)


def generate():
    with open(EXPORTS_JSON) as f:
        available_syms = set(json.load(f))

    tu = cl.TranslationUnit.from_source(
        filename=DIR / 'generate_binding_targets.h',
        args=[
            "-x", "c++",
            f"-I{INCLUDE_DIR}",
        ],
    )

    found_err = False
    for d in tu.diagnostics:
        print(d)
        if d.severity == d.Error:
            found_err = True

    if found_err:
        exit(1)

    info = SearchInfo()
    search(tu.cursor, '', info)

    include_strings = ""

    for header in sorted(info.required_headers):
        # Filter out Windows SDK headers etc
        if INCLUDE_DIR not in header.parents:
            continue

        rel = header.relative_to(INCLUDE_DIR)
        forwardstroke_path = str(rel).replace('\\', '/')
        include_strings += f'#include "{forwardstroke_path}"\n'

    method_list_str = ""
    struct_entry_str = ""
    init_func_body = ""

    for method in info.functions:
        if method.namespace == '':
            fqn = method.name
        else:
            fqn = method.namespace + '::' + method.name

        if method.mangled_name not in available_syms:
            msg = f"Excluding non-exported function: {method.mangled_name}"
            print(msg)
            info.skip_messages += f"// {msg}\n"
            continue

        effective_args = []
        for arg in method.args:
            arg_name = arg.name
            if not arg_name:
                arg_name = f'anon_arg_{len(effective_args) + 1}'
            effective_args.append(Parameter(arg_name, arg.type))

        param_str = ', '.join([f'{param.type} {param.name}' for param in effective_args])
        args_str = ', '.join([param.name for param in effective_args])

        method_list_str += f"""
{method.ret} {fqn}({param_str}) {{
    // {method.mangled_name}
    return WWISE_FUNCTION_TABLE.{method.var_name()}({args_str});
}}
""".strip() + "\n"

        struct_entry_str += (" " * 4) + f"{method.ret} (*{method.var_name()})({param_str});\n"

        init_func_body += f"""
    WWISE_FUNCTION_TABLE.{method.var_name()} = (decltype(WWISE_FUNCTION_TABLE.{method.var_name()})) loadFn("{method.mangled_name}");
    if (!WWISE_FUNCTION_TABLE.{method.var_name()})
        return "{method.mangled_name}";
"""

    # print(method_list_str)

    return info, f"""
// AUTO-GENERATED BY generate_bindings.py - DO NOT EDIT

{info.skip_messages}

{include_strings}

struct WwiseFunctionTable {{
{struct_entry_str}
}} WWISE_FUNCTION_TABLE = {{}};

const char* SBLT_WWISE_INIT_FUNC_TABLE(void* (*loadFn)(const char*)) {{
{init_func_body}
    return nullptr; // Success
}}

{method_list_str}
""".strip()


def load_symbols(exe_path):
    # Loading the entire EXE is quite slow, only load the export directory since that's the only part we want
    exe = pefile.PE(name=exe_path, fast_load=True)
    directories = [pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']]
    exe.parse_data_directories(directories)

    exports: pefile.ExportDirData = exe.DIRECTORY_ENTRY_EXPORT
    symbols: list[str] = []
    for export in exports.symbols:
        symbols.append(export.name.decode('utf-8'))

    return symbols


def main():
    argparser = argparse.ArgumentParser()
    argparser.add_argument('--output', type=str)
    argparser.add_argument('--exe', type=str)
    argparser.add_argument('--update-exports-json', action='store_true')
    args = argparser.parse_args()

    symbols = None
    if args.exe:
        symbols = load_symbols(args.exe)

        # Save a JSON of all the exported symbols, so we only attempt to load the ones that are actually present.
        if args.update_exports_json:
            with open(EXPORTS_JSON, 'w') as f:
                f.write(json.dumps(symbols, indent=4))

    info, s = generate()

    if symbols:
        for fn in info.functions:
            if fn.mangled_name not in symbols:
                print(f"Missing symbol: {fn.mangled_name}")
            else:
                print(f"Valid symbol: {fn.mangled_name}")

    if args.output:
        with open(args.output, 'w') as f:
            f.write(s)
    else:
        print(s)


if __name__ == '__main__':
    main()
