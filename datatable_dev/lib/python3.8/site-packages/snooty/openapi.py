import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import (
    Any,
    Callable,
    Dict,
    Generator,
    List,
    MutableSequence,
    Optional,
    Set,
    TextIO,
    Tuple,
)

import fett
import yaml

from . import n
from .diagnostics import CannotRenderOpenAPI, Diagnostic
from .page import Page
from .types import EmbeddedRstParser

# As of 2018-04, the OpenAPI Python libraries are immature and lack the
# features we need. This extension contains a quick-and-dirty hacky partial
# implementation of OpenAPI 3.0. It requires valid OpenAPI 3.0 input: it
# does no schema validation.


class DictLike:
    """Our templating engine accesses properties via indexing. This class
       mixin creates a __getitem__ wrapper around getattr."""

    def __getitem__(self, key: str) -> object:
        return getattr(self, key)


def deduce_type(obj: Dict[str, Any]) -> str:
    """Return the type of a schema definition."""
    data_type: str = obj.get("type", None)

    if data_type is not None:
        return data_type

    if "items" in obj:
        return "array"

    if "properties" in obj:
        return "object"

    return "any"


@dataclass
class TagDefinition(DictLike):
    """Resources are grouped by logical "tags"."""

    name: str
    title: str
    operations: List[Any] = field(default_factory=list)


@dataclass
class FieldDescription(DictLike):
    """A field in a request body, response, or parameter."""

    name: str
    description: str
    required: bool
    data_type: str
    enum: List[str]

    @classmethod
    def load(
        cls,
        data: Dict[str, Any],
        name: Optional[str] = None,
        required: Optional[bool] = None,
    ) -> "FieldDescription":
        # Merge the schema property, if there is one
        if "schema" in data:
            for key, value in data["schema"].items():
                data[key] = value

        return cls(
            name if name is not None else data["name"],
            data.get("description", ""),
            required if required is not None else data.get("required", False),
            deduce_type(data),
            data.get("enum", []),
        )


HTTP_VERBS = (
    "post",
    "get",
    "put",
    "patch",
    "delete",
    "options",
    "connect",
    "trace",
    "head",
)

fett.Template.FILTERS["values"] = lambda val: val.values()  # type: ignore

PARAMETER_TEMPLATE = fett.Template(
    """
{{ title }}
*****************

.. list-table::
   :header-rows: 1
   :widths: 20 15 15 50

   * - Name
     - Type
     - Necessity
     - Description

{{ for parameter in parameters }}

   * - ``{{ parameter.name }}``
     - {{ parameter.data_type }}
     - {{ if parameter.required }}required{{ else }}optional{{ end }}
     - {{ parameter.description }}{{ if parameter.enum }}

       Possible Values:

       {{ for val in parameter.enum }}
       - ``{{ val }}``

       {{ end }}
       {{ end }}

{{ end }}
"""
)

OPENAPI_TEMPLATE = fett.Template(
    """
{{ if servers }}
Base URL
~~~~~~~~

{{ for server in servers }}
.. code-block:: none

   {{ server.url }}
   
{{ server.description }}
{{ end }}
{{ end }}

{{ for tag in tags }}
{{ tag.title }}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

{{ for operation in tag.operations }}

.. operation::
   :hash: {{ operation.hash }}
   :method: {{ operation.method }}
   :path: {{ operation.path }}

   {{ operation.summary }}

   {{ if operation.description }}
   Description
   ***********

   {{ operation.description }}
   {{ end }}

   {{ operation.path_parameters }}

   {{ operation.query_parameters }}

   {{ operation.header_parameters }}

   {{ operation.cookie_parameters }}

   {{ if operation.requestBody }}
   
   Request Body :required:`{{ operation.requestBody.required }}`
   *********************************

   {{ operation.requestBody.description }}

   .. code-block:: json

      {{ operation.requestBody.jsonSchema }}

   .. list-table::
      :header-rows: 1
      :widths: 20 15 15 50

      * - Field
        - Type
        - Necessity
        - Description

   {{ for field in operation.requestBody.jsonFields }}

      * - ``{{ field.name }}``
        - {{ field.data_type }}
        - {{ if field.required }}required{{ else }}optional{{ end }}
        - {{ field.description }}{{ if field.enum }}

          Possible Values:

          {{ for val in field.enum }}
          - ``{{ val }}``

          {{ end }}
          {{ end }}

   {{ end }}

   {{ end }}

   Responses
   *********

   {{ for response in operation.responses values }}
   ``{{ response.code }}``: {{ response.description }}

   {{ if response.jsonSchema }}
   .. code-block:: json

      {{ response.jsonSchema }}

   {{ if response.jsonFields }}
   .. list-table::
      :header-rows: 1
      :widths: 35 15 50

      * - Field
        - Type
        - Description

   {{ for field in response.jsonFields }}

      * - ``{{ field.name }}``
        - {{ field.data_type }}
        - {{ field.description }}{{ if field.enum }}

          Possible Values:

          {{ for val in field.enum }}
          - ``{{ val }}``

          {{ end }}
{{ end }}
{{ end }}

{{ end }}
{{ end }}
{{ end }}

{{ end }}

{{ end }}
"""
)


# JavaScript Object Notation (JSON) Pointer
# See https://tools.ietf.org/html/rfc6901
def encode_json_pointer(ptr: str) -> str:
    return ptr.replace("~", "~0").replace("/", "~1")


def decode_json_pointer(ptr: str) -> str:
    return ptr.replace("~1", "/").replace("~0", "~")


def dereference_json_pointer(root: n.SerializableType, ptr: str) -> Dict[str, Any]:
    """Given a dictionary or list, return the element referred to by the
       given JSON pointer (RFC-6901)."""
    cursor = root
    components = ptr.lstrip("#").lstrip("/").split("/")
    for component in components:
        component = decode_json_pointer(component)
        if isinstance(cursor, List):
            if component == "-":
                # "-" specifies the imaginary element *after* the last element of an array.
                # We don't need to support this, so... we don't.
                raise NotImplementedError('"-" list components not supported')

            component_int = int(component)
            cursor = cursor[component_int]
        elif not isinstance(cursor, Dict):
            raise ValueError("Invalid entry type")
        else:
            cursor = cursor[component]

    assert isinstance(cursor, Dict)
    return cursor


def schema_as_json(content_schema: Dict[str, n.SerializableType]) -> n.SerializableType:
    """Create a JSON document describing the shape of an OpenAPI schema."""
    if "properties" in content_schema:
        current: Dict[str, Any] = {}
        properties = content_schema["properties"]
        assert isinstance(properties, Dict)
        for prop, options in properties.items():
            if "type" not in options:
                current[prop] = "Any"
            elif options["type"] == "object":
                current[prop] = schema_as_json(options)
            elif options["type"] == "array":
                current[prop] = [schema_as_json(options["items"])]
            else:
                current[prop] = options["type"]

        return current

    if "items" in content_schema:
        assert isinstance(content_schema["items"], Dict)
        return [schema_as_json(content_schema["items"])]

    return content_schema["type"]


def schema_as_fieldlist(content_schema: Dict[str, Any], path: str = "") -> List[Any]:
    """Return a list of OpenAPI schema property descriptions."""
    fields = []

    if "properties" in content_schema:
        required_fields = content_schema.get("required", ())

        for prop, options in content_schema["properties"].items():
            new_path = path + "." + prop if path else prop
            required = (
                options["required"]
                if "required" in options
                else prop in required_fields
            )

            if "type" not in options:
                fields.append(FieldDescription.load(options, new_path, required))
            elif options["type"] == "object":
                fields.append(FieldDescription.load(options, new_path, required))
                fields.extend(schema_as_fieldlist(options, path=new_path))
            elif options["type"] == "array":
                fields.append(FieldDescription.load(options, new_path, required))
                fields.extend(
                    schema_as_fieldlist(options["items"], path=new_path + ".[]")
                )
            else:
                fields.append(FieldDescription.load(options, new_path, required))

    if "items" in content_schema:
        new_path = path + "." + "[]" if path else "[]"
        content_schema["type"] = "array of {}s".format(
            deduce_type(content_schema["items"])
        )
        fields.append(FieldDescription.load(content_schema, new_path))
        fields.extend(schema_as_fieldlist(content_schema["items"], path=new_path))

    return fields


def process_parameters(
    endpoint: Dict[str, Any], operation: Dict[str, Any]
) -> Dict[str, str]:
    """Integrate an operation's parameters with the endpoint's shared
       set of parameters, and return a dictionary with two keys:
       * path_parameters
       * query_parameters
       * header_parameters
       * cookie_parameters"""
    all_parameters = endpoint.get("parameters", []) + operation.get("parameters", [])
    path_parameters: List[FieldDescription] = []
    query_parameters: List[FieldDescription] = []
    header_parameters: List[FieldDescription] = []
    cookie_parameters: List[FieldDescription] = []

    parameter_types = {
        "path": ("path_parameters", "Path Parameters", path_parameters),
        "query": ("query_parameters", "Query Parameters", query_parameters),
        "header": ("header_parameters", "Header Parameters", header_parameters),
        "cookie": ("cookie_parameters", "Cookie Parameters", cookie_parameters),
    }

    for parameter in all_parameters:
        parameter_types[parameter["in"]][2].append(FieldDescription.load(parameter))

    result = {}
    for name, title, parameters in parameter_types.values():
        result[name] = (
            PARAMETER_TEMPLATE.render({"title": title, "parameters": parameters})
            if parameters
            else ""
        )

    return result


class OpenAPI:
    __slots__ = ("data", "tags", "diagnostics")

    def __init__(self, data: Dict[str, Any]) -> None:
        self.data: Dict[str, Any] = data
        self.tags: Dict[str, TagDefinition] = {}
        self.diagnostics: List[Diagnostic] = []

        for tag_definition in self.data["tags"]:
            self.tags[tag_definition["name"]] = TagDefinition(
                tag_definition["name"], tag_definition.get("description", ""), []
            )

        # Substitute refs
        stack: List[Tuple[Dict[str, Any], str, Dict[str, Any]]] = [
            ({}, "<root>", self.data)
        ]
        while stack:
            parent, key, cursor = stack.pop()
            if isinstance(cursor, Dict):
                cursor = self.dereference(cursor)
                parent[key] = cursor

                stack.extend((cursor, subkey, x) for subkey, x in cursor.items())
            elif isinstance(cursor, list):
                stack.extend((cursor, i, x) for i, x in enumerate(cursor))

        # Set up our operations
        for method, path, methods in self.resources():
            resource = methods[method]
            resource.update(
                {
                    "method": method,
                    "path": path,
                    "hash": "{}-{}".format(method, path).lower(),
                }
            )

            resource.setdefault("summary", "")
            resource.setdefault("description", "")
            resource.setdefault("requestBody", None)

            security_schemas = self.get_security_schemas(resource)
            if security_schemas:
                resource.setdefault("parameters", []).append(
                    {
                        "name": "Authorization",
                        "description": security_schemas[0].get("description", ""),
                        "in": "header",
                        "required": True,
                        "schema": {"type": "string"},
                    }
                )

            for code, response in resource["responses"].items():
                response.update({"code": code, "jsonSchema": None, "jsonFields": []})

                if "content" in response and "application/json" in response["content"]:
                    json_schema = response["content"]["application/json"]["schema"]
                    response["jsonSchema"] = json.dumps(
                        schema_as_json(json_schema), indent=2
                    )
                    response["jsonFields"] = schema_as_fieldlist(json_schema)

            if (
                resource["requestBody"] is not None
                and "application/json" in resource["requestBody"]["content"]
            ):
                resource["requestBody"].setdefault("required", False)
                resource["requestBody"].setdefault("description", "")
                json_schema = resource["requestBody"]["content"]["application/json"][
                    "schema"
                ]
                resource["requestBody"]["jsonSchema"] = json.dumps(
                    schema_as_json(json_schema), indent=2
                )
                resource["requestBody"]["jsonFields"] = schema_as_fieldlist(json_schema)

            resource.update(process_parameters(methods, resource))

            for tag in resource.get("tags", ()):
                if tag not in self.tags:
                    self.tags[tag] = TagDefinition(tag, "", [])

                self.tags[tag].operations.append(resource)

    def dereference(
        self, val: Dict[str, Any], loop_set: Optional[Set[int]] = None
    ) -> Dict[str, Any]:
        """Dereference a $ref JSON pointer."""
        if not isinstance(val, Dict):
            return val

        if len(val) == 1 and "$ref" in val:
            if loop_set is None:
                loop_set = set()

            if id(val) in loop_set:
                raise ValueError("$ref loop detected")

            val = dereference_json_pointer(self.data, val["$ref"])
            loop_set.add(id(val))
            return self.dereference(val, loop_set)

        return val

    def resources(self) -> Generator[Tuple[str, str, Dict[str, Any]], None, None]:
        """Enumerate resources listed within this OpenAPI tree."""
        for path, methods in self.data["paths"].items():
            for method in methods:
                if method.lower() not in HTTP_VERBS:
                    continue

                yield method, path, methods

    def get_security_schemas(self, operation: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Return the security schema definitions associated with an
           operation definition."""
        security_schemas = operation.get("security", None)

        if security_schemas is None:
            security_schemas = self.data.get("security", [])

        # Look up the schema definition for each name
        result = []
        for security_schema in security_schemas:
            for security_name in security_schema:
                result.append(self.data["components"]["securitySchemes"][security_name])

        return result

    @classmethod
    def load(cls, data: TextIO) -> "OpenAPI":
        """Load an OpenAPI file stream."""
        return cls(yaml.safe_load(data))

    def to_ast(
        self,
        source_path: Path,
        page_factory: Callable[[], Tuple[Page, EmbeddedRstParser]],
    ) -> Tuple[MutableSequence[n.Node], List[Diagnostic]]:
        try:
            rendered = OPENAPI_TEMPLATE.render(
                {"tags": self.tags.values(), "servers": self.data["servers"]}
            )
        except Exception as error:
            self.diagnostics.append(CannotRenderOpenAPI(source_path, str(error), 0))
            return [], self.diagnostics

        _, rst_parser = page_factory()
        openapi_ast = rst_parser.parse_block(rendered, 0)
        return openapi_ast, self.diagnostics
