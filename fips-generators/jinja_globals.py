"""
This module provides all our custom global functions for the Jinja2 environment.

Any globals defined in this file are automatically imported as globals in Jinja2.
Anything that shouldn't be imported should have a name that starts with an underscore.
"""

import types as _types

from utilities import union_tag
from utilities import object_empty
from utilities import object_items
from utilities import object_keys
from utilities import ordered_object_empty
from utilities import ordered_object_items
from utilities import ordered_object_keys
from utilities import type_schemas_definition_order
from utilities import unique_field_names

def tag_schema_for_union(union):
    """Generate the enum type to represent the tag for a union type."""
    return \
        _types.SimpleNamespace(
            doc="tag type for " + union.name,
            name=union.name + "_tag",
            schema=_types.SimpleNamespace(
                enum=[
                    _types.SimpleNamespace(**{name: _types.SimpleNamespace(doc=info.doc)})
                    for name, info in ordered_object_items(union.schema.union)]))
