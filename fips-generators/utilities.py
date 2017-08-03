"""This module defines various useful CRADLE utilities."""

from collections import OrderedDict

def union_tag(obj):
    """Get the tag of a union object."""
    fields = list(obj.__dict__)
    assert len(fields) == 1, "unions should only have one field"
    return fields[0]

# Add some helper functions for working with our YAML objects.
object_items = lambda obj: obj.__dict__.items()
object_keys = lambda obj: obj.__dict__.keys()
object_empty = lambda obj: not obj.__dict__

# Add additional helpers for working with our YAML 'ordered object'.
# These are lists where each item is an object with a single field (likely a name or identifier).
# They are functionally equivalent to objects except that they preserve order.
ordered_object_items = lambda obj: [(union_tag(i), getattr(i, union_tag(i))) for i in obj]
ordered_object_keys = lambda obj: [union_tag(i) for i in obj]
ordered_object_empty = lambda obj: not obj

def type_schemas_definition_order(type_dict):
    """Given a type schema dictionary, generate a working definition order for C++."""

    type_order = OrderedDict()

    def add_referenced_types(schema):
        """Add any types referenced within the given schema to :type_order."""

        def add_referenced_types_in_map(map_info):
            add_referenced_types(map_info.key_schema)
            add_referenced_types(map_info.value_schema)

        def add_referenced_types_in_structure(structure_info):
            for _, field_info in ordered_object_items(structure_info):
                add_referenced_types(field_info.schema)

        def add_referenced_types_in_named(named):
            if named.name in type_dict:
                add_type_tree(type_dict[named.name])

        cases = {
            "nil": lambda _: None,
            "boolean": lambda _: None,
            "datetime": lambda _: None,
            "integer": lambda _: None,
            "float": lambda _: None,
            "string": lambda _: None,
            "blob": lambda _: None,
            "optional": lambda t: add_referenced_types(t),
            "array": lambda a: add_referenced_types(a.element_schema),
            "map": add_referenced_types_in_map,
            "reference": lambda _: None,
            "named": add_referenced_types_in_named,
            "union": lambda _: None, # Unions can be defined before the types they reference.
            "structure": add_referenced_types_in_structure,
            "enum": lambda _: None,
            "dynamic": lambda t: None
        }

        tag = union_tag(schema)
        cases[tag](getattr(schema, tag))

    def add_type_tree(type_info):
        """Recursively add the given type and any types referenced within it to :type_order."""
        add_referenced_types(type_info.schema)
        type_order[type_info.name] = True

    for _, type_info in type_dict.items():
        add_type_tree(type_info)

    return type_order

def unique_field_names(types):
    """Given a dictionary of types from an API, generate a list of unique
       field names that occur within any structures in :types."""
    field_names = set()
    for _, t in types.items():
        if union_tag(t.schema) == 'structure':
            for name, _ in ordered_object_items(t.schema.structure):
                field_names.add(name)
    return sorted(field_names)
