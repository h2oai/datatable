/*
See: https://docs.python.org/3/extending/embedding.html

$LLVM6/bin/clang \
  -I/Library/Frameworks/Python.framework/Versions/3.6/include/python3.6m
  -L/Library/Frameworks/Python.framework/Versions/3.6/lib/python3.6/config-3.6m-darwin -lpython3.6m -ldl -framework CoreFoundation \
  -fsanitize=address -o asan-python asan-python.c
*/
#include <Python.h>
#include <dirent.h>

int main(int argc, char** argv)
{
  wchar_t** wargv = malloc(sizeof(wchar_t*) * (argc + 1));  // new ref
  for (int i = 0; i < argc; ++i) {
    wargv[i] = Py_DecodeLocale(argv[i], NULL);  // to free use PyMem_RawFree
  }
  wargv[argc] = NULL;

  char* venv0 = getenv("DT_ASAN_TARGETDIR");  // borrowed ref
  if (!venv0) {
    fprintf(stderr, "Environment variable DT_ASAN_TARGETDIR is missing\n");
    exit(1);
  }
  char* venv1 = realpath(venv0, NULL);  // new ref

  wchar_t* wpath0 = Py_GetPath();
  char* pypath0 = malloc(wcslen(wpath0) + 1);
  sprintf(pypath0, "%S", wpath0);

  char* path = malloc(strlen(venv1) + strlen(pypath0) + 2);  // new ref
  sprintf(path, "%s:%s", venv1, pypath0);
  wchar_t* wpath = Py_DecodeLocale(path, NULL);
  Py_SetPath(wpath);

  if (argc == 1) printf("[my-python: PATH=%s]\n", path);
  int ret = Py_Main(argc, wargv);

  // Clean-up allocated resources, so that leak sanitizer wouldn't report them
  free(venv1);
  free(path);
  for (int i = 0; i < argc; ++i) PyMem_RawFree(wargv[i]);
  free(wargv);
  return ret;
}
