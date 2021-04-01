# Fireplace 🔥

Enjoy a cozy fireplace in your terminal.

![A gif of what to expect](demo.gif?raw=true "Cozy")

## Using the docker image

`docker run -it --rm jwestp/fireplace`

## Options

```
-c character	An ASCII character to draw the flames. Default is '@'.
-h		Print this message.
-f framerate	Set the framerate in frames/sec. Default is 20.
		A framerate of zero will make frames spit out as soon as they are ready.
-t temp		Set the maximum temperature of the flames. Default is 10.
		A higher temp means taller flames.

Use the up and down arrow keys to change the temperature.
Press q at any time to douse the flames.
```

## Manually building and running the docker image

* `docker build . -t fireplace`
* `docker run -it --rm fireplace`

## Manual building and running locally

* Install `ncurses` including the development packages (usually called libncurses5-dev or something similar)
* `make`
* `./fireplace`
