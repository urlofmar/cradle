"""
This module provides all our custom global functions for the Jinja2 environment.

Any globals defined in this file are automatically imported as globals in Jinja2.
Anything that shouldn't be imported should have a name that starts with an underscore.
"""

import types as _types

from utilities import union_tag

def tag_schema_for_union(union):
    """Generate the enum type to represent the tag for a union type."""
    values = {}
    for name, info in union.schema.union_type.members.__dict__.items():
        values[name] = _types.SimpleNamespace(
            description=info.description)
    return _types.SimpleNamespace(
        description="tag type for " + union.name,
        name=union.name + "_tag",
        schema=_types.SimpleNamespace(
            enum_type=_types.SimpleNamespace(
                values=_types.SimpleNamespace(**values))))

# Add some helper functions for working with our JSON objects.
object_items = lambda obj: obj.__dict__.items()
object_keys = lambda obj: obj.__dict__.keys()
object_empty = lambda obj: not obj.__dict__

from utilities import type_schemas_definition_order
