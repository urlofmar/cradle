# CRADLE documentation

## Contents
* [Introduction](intro.md)
* [Retrieve an immutable object](iss_object.md)


## Setup
This documentation builds on Markdown and [PlantUML](https://plantuml.com/).

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

Then start the server by

```
java -jar plantuml.jar -picoweb
```

The server can now be reached on port 8080.

The PlantUML extension is "PlantUML" by jebbs. On Windows, it should be installed under WSL.
The server should be configured in the extension's settings (again, in the WSL tab for Windows):

* "Plantuml: Render" should be set to `PlantUMLServer`
* "Plantuml: Render" should be set to `http://localhost:8080`

If all works well, a preview of a `.md` file should render both the Markdown and inlined
PlantUML content.
