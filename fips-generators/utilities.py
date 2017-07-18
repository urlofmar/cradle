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

    def add_type_tree(type_info):
        """Recursively add the given type and any types included within it to :type_order."""
        if union_tag(type_info.schema) == "structure_type":
            for _, field_info in type_info.schema.structure_type.fields.__dict__.items():
                if union_tag(field_info.schema) == "named_type":
                    add_type_tree(type_dict[field_info.schema.named_type.name])
        type_order[type_info.name] = True

    for _, type_info in type_dict.items():
        add_type_tree(type_info)

    return type_order
