"""This module defines various useful CRADLE utilities."""

from collections import OrderedDict

def union_tag(obj):
    """Get the tag of a union object."""
    fields = list(obj.__dict__)
    assert len(fields) == 1, "unions should only have one field"
    return fields[0]

def type_schemas_definition_order(type_dict):
    """Given a type schema dictionary, generate a working definition order for C++."""

    type_order = OrderedDict()

    def add_referenced_types(schema):
        """Add any types referenced within the given schema to :type_order."""

        def add_referenced_types_in_map(map_info):
            add_referenced_types(map_info.key_schema)
            add_referenced_types(map_info.value_schema)

        cases = {
            "nil_type": lambda _: None,
            "boolean_type": lambda _: None,
            "datetime_type": lambda _: None,
            "integer_type": lambda _: None,
            "float_type": lambda _: None,
            "string_type": lambda _: None,
            "blob_type": lambda _: None,
            "optional_type": lambda t: add_referenced_types(t),
            "array_type": lambda a: add_referenced_types(a.element_schema),
            "map_type": add_referenced_types_in_map,
            "reference_type": lambda _: None,
            "named_type": lambda t: add_type_tree(type_dict[schema.named_type.name]),
            "dynamic_type": lambda t: None
        }

        tag = union_tag(schema)
        cases[tag](getattr(schema, tag))

    def add_type_tree(type_info):
        """Recursively add the given type and any types referenced within it to :type_order."""
        if union_tag(type_info.schema) == "structure_type":
            for _, field_info in type_info.schema.structure_type.fields.__dict__.items():
                add_referenced_types(field_info.schema)
        type_order[type_info.name] = True

    for _, type_info in type_dict.items():
        add_type_tree(type_info)

    return type_order
