# tiny_view

The tiny image viewer written in C using SDL2. Written in a few hours

## Controls

arrow right / l -> next image (in the same folder)
arrow left / h -> previous image (in the same folder)
arrow up / k -> zoom in
arrow down / j -> zoom out

w -> move view up / picture down
s -> move view down / picture up
a -> move view left / picture right
d -> move view right / picture left

: -> show command bar 
	- the program specified will be executed with the filepath of the image as arg1
  	- Special CMDs
	- q -> exit program for all vim users who cant escape

CTRL+q / CTRL+C -> exit program

## Config
- only config implemented right now is the ttf font for the cmd line.
- can be changed in the tv bash script. the font there is the first Argument in the tiny_view program call

## Usage
- directly call the binary and use the defaul font in "/usr/share/fonts/gnu-free/FreeSans.otf" (not provided in package) with : `tiny-view /path/to/image.jpg`
- call the startup script that sets the font to anything you want with : `tv /path/to/image.jpg`

## FYI
- someone would call the zoom wonky since its not centered zoom but zooming in the top left corner, but somehow i like this

