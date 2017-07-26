# Import the fips generator utilities.
import genutil

# fips uses this to version the generator.
Version = 9

# Note that other imports are deferred to the points where they're needed as
# this seems to speed up build times in cases where the generated files are
# up-to-date (and those imports aren't needed).

def initialize_jinja_env():
    """Set up the Jinja2 environment."""

    import jinja2
    import os

    import jinja_filters
    import jinja_globals

    # Templates are stored in a subdirectory of the directory where this script lives.
    template_dir = \
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "cpp_api_templates")

    jinja_env = \
        jinja2.Environment(
            loader=jinja2.FileSystemLoader(template_dir),
            # Disable autoescaping.
            autoescape=lambda template_name: False,
            # Set sane whitespace policies.
            keep_trailing_newline=True,
            lstrip_blocks=True,
            trim_blocks=True)

    # Import the symbols defined in the filters and globals modules.
    jinja_env.filters.update(
        {k: v for k, v in jinja_filters.__dict__.items() if not k.startswith('_')})
    jinja_env.globals.update(
        {k: v for k, v in jinja_globals.__dict__.items() if not k.startswith('_')})

    return jinja_env

def invoke_template(jinja_env, template_file, output_file, var_dict):
    """Invoke a Jinja2 template and stream its output to a file."""
    jinja_env.get_template(template_file).stream(var_dict).dump(output_file)

def generate(api_file_path, source_file_path, header_file_path, args):
    """Generate C++ source and header files from an API file."""
    if genutil.isDirty(Version, [api_file_path], [source_file_path, header_file_path]):
        import json
        from types import SimpleNamespace

        jinja_env = initialize_jinja_env()

        # Parse the JSON API as an object.
        with open(api_file_path, 'r') as api_file:
            api = json.loads(api_file.read(), object_hook=lambda d: SimpleNamespace(**d))

        # Convert the type list to a dictionary.
        api.types = {type_info.name: type_info for type_info in api.types}

        invoke_template(
            jinja_env,
            'hpp_file.j2',
            header_file_path,
            {**args, 'api': api, 'version': Version})

        invoke_template(
            jinja_env,
            'cpp_file.j2',
            source_file_path,
            {**args, 'api': api, 'version': Version, 'header_file_path': header_file_path})
