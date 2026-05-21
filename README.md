# name_hash_bruteforcer
A bruteforcer for names of unknown WoW root file entries.

## Examples
Note that all elapsed durations shown in these examples currently include some of the initialization time for the program. The hash rates are calculated from this number, so they will be particularly inaccurate for short durations like this.

### Finding names for a specific hash using a pattern
Unknown characters can be specified using either `*` or `%` characters.
```
>bruteforcer -n 8aaf1e907b81e722 -p Interface/Cinematics/Logo_****.avi
Interface/Cinematics/Logo_1024.avi
[100.00%] 00:00:00.108 elapsed, 21.42 Mh/s
>bruteforcer -n 8aaf1e907b81e722 -p Interface/Cinematics/Logo_%%%%.avi
Interface/Cinematics/Logo_1024.avi
[100.00%] 00:00:00.109 elapsed, 21.22 Mh/s
```

### Using a file to compare against multiple hahses at once
**test/lookup1.csv**
```
5917326;5c3c172fb5afe9a8
5917328;1a7f618193af4317
```
```
>bruteforcer -n test/lookup1.csv -p CREATURE/MAGMASPHERE/MagmaSphere_****.blp
5917326;CREATURE/MAGMASPHERE/MagmaSphere_rock.blp
5917328;CREATURE/MAGMASPHERE/MagmaSphere_drip.blp
[100.00%] 00:00:00.107 elapsed, 21.62 Mh/s
```

### Using mirrored wildcards for patterns where a section of text is expected to repeat
When `*` and `%` are used in the same pattern, all instances of whichever wildcard appears fewer times will match the sequence given by the other wildcard. In this example, the value of `****` is always equal to the value of `%%%%`. If the number of `*`s is not equal to the number of `%`s, then only the leftmost sequences of equal length will match.
```
>bruteforcer -n d3a6007a88ee14fc -p world/maps/****/%%%%.wdt
world/maps/2875/2875.wdt
[100.00%] 00:00:00.107 elapsed, 21.62 Mh/s
```

### Changing the alphabet
By default, the alphabet used is 39 characters long `ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- `. Sometimes, you will want to use a shorter alphabet to speed up results. In this example, there are 8 `*`s and the alphabet contains 10 characters, resulting in `10^8` possible names. This is a very small fraction of the `39^8` names that would need to be checked using the default alphabet.

**test/lookup2.csv**
```
6851560;eb1470de6bc3aa72
6851564;11c0d210c1c704e0
6798793;709dbab1aeebe066
6798801;9436e41913c72fc9
```
```
>bruteforcer -n test/lookup2.csv -p world/maps/****/%%%%_**_**.adt -a 0123456789
6851560;world/maps/2868/2868_41_25.adt
6851564;world/maps/2868/2868_41_26.adt
6798793;world/maps/2868/2868_43_26.adt
6798801;world/maps/2868/2868_43_27.adt
[100.00%] 00:00:00.833 elapsed, 120.05 Mh/s
```

### Using a dictionary
Dictionaries can be provided using the `-d` option and words from them will replace instances of `@` or `#` characters in the pattern. If `@` and `#` are used, the one that appears fewer times will result in a mirrored replacement as described for wildcards above. If multiple dictionaries are provided with multiple instances of the `-d` option, then the Nth wildcard will be replaced with words from the Nth dictionary, or the first dictionary if there are fewer than N dictionaries provided.

**test/dictionary.txt**
```
Logo
1024
asdf
testing
123
MagmaSphere
Rock
Drip
```
```
>bruteforcer -n 8aaf1e907b81e722 -d test/dictionary.txt -p Interface/Cinematics/@_@.avi
Interface/Cinematics/logo_1024.avi
[100.00%] 00:00:00.102 elapsed, 627.45 h/s
```
```
>bruteforcer -n test/lookup1.csv -d test/dictionary.txt -p CREATURE/@/#_@.blp
5917328;CREATURE/magmasphere/magmasphere_drip.blp
5917326;CREATURE/magmasphere/magmasphere_rock.blp
[100.00%] 00:00:00.006 elapsed, 10.67 Kh/s
```

### Checking multiple patterns from a file
Each line of the file should contain one pattern to evaluate. You can use an optional `;` to specify a different alphabet for each pattern. Any patterns without an alphabet will use the default alphabet given by the `-a` option.

**test/patterns.txt**
```
world/maps/Azeroth/Azeroth.***
world/maps/PvPZone**/PvPZone%%.wdt;0123456789
```
**test/lookup3.csv**
```
775970;03cfaa3828852a0c
775971;19a3c4cbc40e9fa2
775969;5696788a36a45354
790469;e36d6cf0db74c1b9
790112;2a4cc86de76dd77c
790291;69b29ae06c12b0b6
790377;d646732c0704b50e
861092;7327b34484cfc0ed
```
```
>bruteforcer -n test/lookup3.csv -f test/patterns.txt
775970;world/maps/Azeroth/Azeroth.wdl
775971;world/maps/Azeroth/Azeroth.wdt
775969;world/maps/Azeroth/Azeroth.tex
790291;world/maps/PvPZone03/PvPZone03.wdt
790469;world/maps/PvPZone05/PvPZone05.wdt
790112;world/maps/PvPZone01/PvPZone01.wdt
790377;world/maps/PvPZone04/PvPZone04.wdt
861092;world/maps/PvPZone02/PvPZone02.wdt
[100.00%] 00:00:00.212 elapsed, 280.28 Kh/s
```

### Using a GPU
For patterns with more than a few unknown characters, it is significantly faster to use a GPU instead of a CPU. You can use the `-g` option to enable GPU support where it's available.
```
>bruteforcer -n 8aaf1e907b81e722 -p Interface/Cinematics/L***_****.avi -g
Interface/Cinematics/Logo_1024.avi
[100.00%] 00:00:04.114 elapsed, 33.36 Gh/s
>bruteforcer -n 8aaf1e907b81e722 -p Interface/Cinematics/L********.avi -g
Interface/Cinematics/Logo_1024.avi
[100.00%] 00:02:35.623 elapsed, 34.39 Gh/s
```

### Use an existing listfile and automatic strategies to try to match new names
This is intended for use with the [community listfile](https://github.com/wowdev/wow-listfile). Currently, this simply checks many combinations of existing directories and filenames contained in the listfile in an attempt to find existing files that were copied to somewhere else without changing the filename. Because a listfile was provided, this will exclude any hashes from the file given with `-n` that are already known.

You can also use these options along with any of the pattern options above to dynamically prepare a list of unknown or incorrect name hashes from the community listfile instead of providing your own list. Note that these files are relatively large, so doing it this way will be less responsive than using your own already filtered list of name hashes.
```
>./bruteforcer -n ../wow-listfile/meta/lookup.csv -l ../listfile-withcapitals.csv
3169803;shaders/raytracing/dx_6_0/TestShader1_Internal.bls
[100.00%] 00:08:31.590 elapsed, 74.27 Mh/s
```
