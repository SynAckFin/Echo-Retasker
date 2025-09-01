# LED Ring

The `ledring` app is only intended as a demonstration and not as a finished product.

## Build

```
make
```

## Usage

```
usage: ledring [options]

Options:
 -h, --help                    This message
 -i, --i2c <device>            i2c device. Default: /dev/i2c-1
 -a, --anim_dir <directory>    Directory with animations. Default: led-animation
 -l, --loops <count>           Number of times to execute animation loop. Default: 10
 ```

## Animmation Files

`ledring` will read any file with a `.anim` extension in the `led-animation` directory. The resulting animation can be executed by using the filename, without the extension, as an argument to `ledring`.

The animation file contains a series of frames and an optional `loop` directive. Anything after a `#`
anywhere on a line is considered a comment.

Each frame starts with a decimal number that indicates the number of 1/100ths of a second the frame should be displayed. This is followed by 12 hexadecimal numbers each of which represents the RGB value of an LED cluster. The hexadecimal numbers **must** be either 3 or 6 digits long to be valid.

Example:
```
#     RGB RGB RGB RGB RGB RGB RGB RGB RGB RGB RGB RGB
10000:F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,
loop
40:808,808,808,808,808,808,808,808,808,808,808,808,
100:F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,
40:808,808,808,808,808,808,808,808,808,808,808,808,
140:101,101,101,101,101,101,101,101,101,101,101,101,
60:F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,F0F,
600:101,101,101,101,101,101,101,101,101,101,101,101,
```
This animation will display a purple ring for 10 seconds (10000/100) before going into a loop
and pulsing until the loop count ends.
In the above axample, `808` is exactly equivalent to `888888` and will set the red, green and blue LED values to `0x88`.
