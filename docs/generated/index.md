# CRADLE documentation

## Contents
* [Introduction](intro.md)
* [Immutable data](data.md)
* [Cache](cache.md)
* [Messages](msg_overview.md)


## Setup
This documentation builds on Markdown and [PlantUML](https://plantuml.com/).

### Visual Studio Code

It is recommended to develop this documentation in Visual Studio Code (VS Code):
the preview functionality from the built-in Markdown extension, plus a PlantUML one,
works really well.

The PlantUML extension requires a connection to a PlantUML server. Setting up a local
one is easy on Linux or WSL:

* Download `plantuml.jar` from the [PlantUML website](https://plantuml.com/).
* Ensure that Java and graphviz are installed, e.g.

```
sudo apt install openjdk-17-jre-headless graphviz
```

* Then start the server by

```
java -jar plantuml.jar -picoweb
```

The server can now be reached on port 8080.

The PlantUML extension is "PlantUML" by jebbs. On Windows, it should be installed under WSL.
The server should be configured in the extension's settings (again, in the WSL tab for Windows):

* "Plantuml: Render" should be set to `PlantUMLServer`
* "Plantuml: Server" should be set to `http://localhost:8080`

If all works well, a preview of a `.md` file should render both the Markdown and inlined
PlantUML content.

### Convert PlantUML content
The Markdown files in the `docs` directory contain inline PlantUML content that
usual Markdown viewers (like GitHub) cannot render. To support these viewers, the PlantUML
fragments are converted to image files (`.svg`), and replaced with
references to those image files.

To achieve this:

```shell
$ cd /path/to/repo/docs
$ rm generated/*  # Optional step, to remove stale files
$ python3 replace-puml.py *.md
```

The modified Markdown files (without PlantUML content), and the image files, end up in the
`generated` subdirectory. These files will be under change control,
and the entrypoint to the generated documentation will be `/path/to/repo/docs/generated/index.md`.
