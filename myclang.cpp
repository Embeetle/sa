// Copyright 2018-2024 Johan Cockx
#include <clang-c/Index.h>
#include <iostream>

std::ostream &operator<<(std::ostream &out, const CXString string)
{
  const char *c_string = clang_getCString(string);
  if (c_string) {
    out << c_string;
  } else {
    out << "(null)";
  }
  return out;
}

std::ostream &operator<<(std::ostream &out, CXCursorKind kind)
{
  CXString name = clang_getCursorKindSpelling(kind);
  out << name;
  clang_disposeString(name);
  return out;
}

std::ostream &operator<<(std::ostream &out, CXLinkageKind kind)
{
  switch (kind) {
    case CXLinkage_Invalid: out << "invalid"; break;
    case CXLinkage_NoLinkage: out << "none"; break;
    case CXLinkage_Internal: out << "internal"; break;
    case CXLinkage_UniqueExternal: out << "anonymous"; break;
    case CXLinkage_External: out << "external"; break;
  }
  return out;
}

std::ostream &operator<<(std::ostream &out, CXVisibilityKind kind)
{
  switch (kind) {
    case CXVisibility_Invalid: out << "invalid"; break;
    case CXVisibility_Hidden: out << "hidden"; break;
    case CXVisibility_Protected: out << "protected"; break;
    case CXVisibility_Default: out << "default"; break;
  }
  return out;
}

std::ostream &operator<<(std::ostream &out, CX_StorageClass kind)
{
  switch (kind) {
    case CX_SC_Invalid: out << "invalid"; break;
    case CX_SC_None: out << "none"; break;
    case CX_SC_Extern: out << "extern"; break;
    case CX_SC_Static: out << "static"; break;
    case CX_SC_PrivateExtern: out << "private-extern"; break;
    case CX_SC_OpenCLWorkGroupLocal: out << "open-CL-work-group-local"; break;
    case CX_SC_Auto: out << "auto"; break;
    case CX_SC_Register: out << "register"; break;
  }
  return out;
}

std::ostream &operator<<(std::ostream &out, CXObjCPropertyAttrKind kind)
{
  switch (kind) {
    case CXObjCPropertyAttr_noattr: out << "noattr"; break;
    case CXObjCPropertyAttr_readonly: out << "readonly"; break;
    case CXObjCPropertyAttr_getter: out << "getter"; break;
    case CXObjCPropertyAttr_assign: out << "assign"; break;
    case CXObjCPropertyAttr_readwrite: out << "readwrite"; break;
    case CXObjCPropertyAttr_retain: out << "retain"; break;
    case CXObjCPropertyAttr_copy: out << "copy"; break;
    case CXObjCPropertyAttr_nonatomic: out << "nonatomic"; break;
    case CXObjCPropertyAttr_setter: out << "setter"; break;
    case CXObjCPropertyAttr_atomic: out << "atomic"; break;
    case CXObjCPropertyAttr_weak: out << "weak"; break;
    case CXObjCPropertyAttr_strong: out << "strong"; break;
    case CXObjCPropertyAttr_unsafe_unretained: out << "unsafe_unretained"; break;
    case CXObjCPropertyAttr_class: out << "class"; break;
  }
  return out;
}

std::ostream &operator<<(std::ostream &out, const CXSourceLocation loc)
{
  CXFile file;
  unsigned line = 0;
  unsigned column = 0;
  unsigned offset = 0;
  // getFileLocation return where a macro was expanded.
  // getSpellingLocation returns where the macro was define.
  // If no macro is involved,  the two are equivalent.
  clang_getFileLocation(loc, &file, &line, &column, &offset);
  if (file) {
    auto filename = clang_getFileName(file);
    out << filename << " line " << line << " col " << column << " @" << offset;
  } else {
    out << "(nowhere)";
  }
  return out;
}
  
std::ostream &operator<<(std::ostream &out, const CXIdxLoc &loc)
{
  out << clang_indexLoc_getCXSourceLocation(loc);
  return out;
}
  
std::ostream &operator<<(std::ostream &out, const CXSourceRange range)
{
  if (!clang_Range_isNull(range)) {
    CXSourceLocation start = clang_getRangeStart(range);
    CXFile start_file;
    unsigned start_line = 0;
    unsigned start_column = 0;
    unsigned start_offset = 0;
    clang_getFileLocation(
      start, &start_file, &start_line, &start_column, &start_offset
    );
    
    CXSourceLocation end = clang_getRangeEnd(range);
    CXFile end_file;
    unsigned end_line = 0;
    unsigned end_column = 0;
    unsigned end_offset = 0;
    clang_getFileLocation(
      end, &end_file, &end_line, &end_column, &end_offset
    );
    if (end_file == start_file) {
      out << start << "+" << (end_offset - start_offset);
    } else {
      out << start << ".." << end;
    }
  }
  return out;
}

std::ostream &operator<<(std::ostream &out, const CXCursor cursor)
{
  CXString name = clang_getCursorDisplayName(cursor);
  //CXString usr = clang_getCursorSpelling(cursor);
  out << clang_getCursorKind(cursor) << "(" << name << ")"
      << " " << clang_getCursorExtent(cursor);
  //clang_disposeString(usr);
  clang_disposeString(name);
  return out;
}

static CXIdxClientFile callback_ppIncludedFile(
  CXClientData client_data,
  const CXIdxIncludedFileInfo *info
)
{
  CXString cx_path = clang_getFileName(info->file);
  const char *path = clang_getCString(cx_path);
  std::cout << "Include " << (path ? path : "<null>") << std::endl;
  clang_disposeString(cx_path);
  return 0;
}

static void callback_indexDeclaration(
  CXClientData client_data,
  const CXIdxDeclInfo *info
)
{
  (void)client_data;
  std::cout << "  `-> "
            << (info->isDefinition ? "definition" : "declaration")
            << " at " << info->cursor
            << " at " << info->loc
            << std::endl;
  std::cout << " name: " << clang_getCursorDisplayName(info->cursor) <<std::endl;
  std::cout << "  USR: " << clang_getCursorUSR(info->cursor) << std::endl;
  
}

// Called to index a occurrence of an entity.
static void callback_indexEntityOccurrence(
  CXClientData client_data,
  const CXIdxEntityRefInfo *
)
{
  (void)client_data;
  std::cout << "callback_indexEntityOccurrence" << std::endl;
}

static enum CXChildVisitResult occurrence_visitor(
  CXCursor cursor,
  CXCursor parent,
  CXClientData client_data
)
{
  std::cout << cursor << "\n";
  return CXChildVisit_Recurse;
}

int main(int argc, char **argv)
{
  (void)callback_indexEntityOccurrence;
  (void)callback_indexDeclaration;
  (void)callback_ppIncludedFile;
  
  CXIndex index = clang_createIndex(1, /*report diagnostics on stderr = */1);
  CXIndexAction index_action = clang_IndexAction_create(index);
  CXTranslationUnit clang_unit = 0;
  IndexerCallbacks callbacks = {
    0,//callback_abortQuery,
    0,//callback_diagnostic,
    0,//callback_enteredMainFile,
    0,//callback_ppIncludedFile,
    0,//callback_importedASTFile,
    0,//callback_startedTranslationUnit,
    0,//callback_indexDeclaration,
    0,//callback_indexEntityOccurrence,
  };
  unsigned clang_error = clang_indexSourceFile(
    index_action,
    0, // client data
    &callbacks, sizeof(callbacks),
    CXIndexOpt_IndexFunctionLocalSymbols
    | CXIndexOpt_IndexImplicitTemplateInstantiations,
    0, // source file, or null if included in args
    argv, argc,
    0, 0, // unsaved files
    &clang_unit, //&translation unit pointer or null
    CXTranslationUnit_KeepGoing | CXTranslationUnit_DetailedPreprocessingRecord
  );
  if (clang_error) {
    const char *clang_message[] = {
      "no error",
      "internal error", // Also returned when main source file cannot be opened
      "crashed",
      "invalid arguments",
      "deserialization error"
    };
    const char *message = clang_message[
      clang_error >= sizeof(clang_message)/sizeof(*clang_message)
      ? 1 : clang_error
    ];
    std::cerr << "Clang error " << clang_error << ": " << message << std::endl;
  } else {
    CXCursor cursor = clang_getTranslationUnitCursor(clang_unit);
    clang_visitChildren(cursor, occurrence_visitor, 0);
  }
  if (clang_unit) {
    clang_disposeTranslationUnit(clang_unit);
  }
  return clang_error;
}
