# Youtube Video Downloader
### Made by: iThief

### Usage:
```
ytdl.exe [mode] [options]
(order of mode and options is not important)
```
### Available modes:
```
-l --list [URL]
```
> Outputs listing of available formats and other meta data. Can be used with *-j*, *-nc*, *-o*, *-fmt*
```
-d --download [URL]
```
> Downloads given video in selected format. Can be combined with *-j*, *-nc*, *-o*, *-fmt*, *-f*
```
-i --interactive [URL]
```
> Interactive version of downloader. URL is optional. Can be combined with *-j*, *-nc*, *-fmt*


### Available options:
```
-nc --no-cache
```
> Do not use cached results and do not create cache files.
```
-j --json
```
> Use JSON format as output. If used in -d mode, output consists of progress and/or error messages.
```
-fmt --fmt-endpoint [URL]
```
> Use custom FMT parsing endpoint. Default behavior is to use built-in parser.
```
-f --format [FORMAT]
```
> Specify which formats to download. See below.
```
-o --output [FILE]
```
> If used in *-d* mode, this represents template for downloaded files. Otherwise it's file to which all output should be stored in. If omitted in *-d* mode, video name is used as a template.
```
-p --peek
```
> If used in *-d* mode, simulates download without actually downloading anything.
```
-cf --cache-file [FILE]
```
> Use cache file. If omitted, default "youtube.cache" file is used
```
-mcs --max-cache-size [SIZE]
```
> Maximum allowed cache size in bytes. If exceeded, cache file is trimmed randomly.


### Formats:
#### Basic syntax:
```
-[A|V|AV][ID]
```
> Download specific format ID as listed in *-l* and *-i* mode.

#### Advanced syntax:
```
(-[A|V|AV][QUALITY][+-]?/[EXTENSION_LIST|\*])*
```
> Generic format. Multiple files can be specified
```
-[A|V|AV]
```
> if "Audio only", "Video only" or "Audio and video" should be queried
```
[QUALITY][+|-]?
```
> String representation of a desired quality. If "+" ("-") is used, query exact quality or higher (lower) if no such quality is available.        /[EXTENSION_LIST*|\*] - List of extensions (formats) that identifies priorities of identical quality results. If * is used, any format is chosen.

#### Format examples:
```
-V360p+/mp4,flv,*
```
> Download video in quality 360p or higher, first look for mp4, then look for flv and then take any available format
```
-A128k-/*
```
> Download any audio file in quality 128k or lower, format doesn't matter
```
-A128k-/*-V1080p/*
```
> Download any audio file in quality 128k or lower, format doesn't matter and then download a video file in quality 1080p, format doesn't matter


### FMT Endpoints:
> Supported are REST endpoints in [maple3142/ytdl](https://github.com/maple3142/ytdl) format.

> Example endpoint: [https://maple3142-ytdl-demo.glitch.me/api](https://maple3142-ytdl-demo.glitch.me/api)

### TODO:
> Speed up downloads (use custom https)

> Add stream muxer