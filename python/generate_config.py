import sys
import os
import yaml

config_template_path = os.path.join(os.path.dirname(__file__), 'config_template.yml')
with open(config_template_path, 'r') as config_template_file:
    config = yaml.load(config_template_file)

config['api_token'] = sys.argv[1]
config['cradle_url'] = 'ws://localhost:41071'

config_file_path = os.path.join(os.path.dirname(__file__), 'config.yml')
with open(config_file_path, 'w') as config_file:
    yaml.dump(config, config_file, default_flow_style=False)
