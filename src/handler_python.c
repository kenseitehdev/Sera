#include "../include/sera.h"
#include <Python.h>
#include <string.h>

static int python_initialized = 0;
static PyObject *routes_dict = NULL;

static PyObject* py_route(PyObject *self, PyObject *args) {
    const char *path;
    PyObject *func;
    if (!PyArg_ParseTuple(args, "sO", &path, &func)) return NULL;
    PyDict_SetItemString(routes_dict, path, func);
    Py_RETURN_NONE;
}

static PyObject* py_forward(PyObject *self, PyObject *args) {
    const char *path;
    if (!PyArg_ParseTuple(args, "s", &path)) return NULL;
    return PyUnicode_FromFormat("__FORWARD__:%s", path);
}

// --- module definition ---
static PyMethodDef SeraMethods[] = {
    {"route", (PyCFunction)py_route, METH_VARARGS, "Register a route"},
    {"forward", (PyCFunction)py_forward, METH_VARARGS, "Forward internally"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef SeraModule = {
    PyModuleDef_HEAD_INIT,
    "sera",
    NULL,
    -1,
    SeraMethods
};
// --------------------------

http_response_t *handle_python(http_request_t *req) {
    if (!python_initialized) {
        Py_Initialize();
        python_initialized = 1;

        PyObject *m = PyModule_Create(&SeraModule);
        PyImport_AddModule("sera");
        Py_INCREF(m);
        PyModule_AddObject(PyImport_AddModule("__main__"), "sera", m);

        routes_dict = PyDict_New();
    }

    FILE *f = fopen("app.py", "r");
    if (!f) return NULL;
    PyRun_SimpleFile(f, "app.py");
    fclose(f);

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(routes_dict, &pos, &key, &value)) {
        const char *pattern = PyUnicode_AsUTF8(key);
        if (strcmp(pattern, req->path) == 0) {
            PyObject *args = PyTuple_Pack(1, PyUnicode_FromString(req->path));
            PyObject *result = PyObject_CallObject(value, args);
            Py_DECREF(args);
            if (result) {
                const char *out = PyUnicode_AsUTF8(result);
                http_response_t *res = malloc(sizeof(http_response_t));
                res->status = 200;
                res->content_type = "text/plain";
                res->body = strdup(out);
                res->body_len = strlen(out);
                return res;
            }
        }
    }

    http_response_t *res = malloc(sizeof(http_response_t));
    res->status = 404;
    res->content_type = "text/plain";
    res->body = strdup("Python route not found");
    res->body_len = strlen(res->body);
    return res;
}
