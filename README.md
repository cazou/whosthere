# whosthere-c

This is a program for esp32 to be hooked into a 4-wire intercom system.
It is written in C and still in heavy development


## Console
A console is available on the default UART. Use the help command to learn about functions.

This is mainly used for developping/debugging

## Talk

The talk function will bind UDP port 5000 and wait for RTP data (don't use extended headers).
The RTP payload has to contain 8 bit uncompressed audio, sampled at the configured
`CONFIG_AUDIO_SAMPLE_RATE` value (default to 44100 Hz).

Such data can be generated with Gstreamer:
```
gst-launch-1.0 -v filesrc location="in.mp3" ! \
    decodebin ! \
    audioresample ! \
    audioconvert ! \
    audio/x-raw,rate=44100,channels=1,channel-mask=1 ! \
    rtpL8pay auto-header-extension=false ! \
    udpsink host=<ESP32_IP> port=5000
```

The audio signal will be rendered on the GPIO25 pin.

## Listen

(TBD)

## Open door

(TBD)

## Notify ringing

(TBD)

## MQTT

(TBD)
