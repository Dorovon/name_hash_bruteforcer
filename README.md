# name_hash_bruteforcer
A bruteforcer for names of unknown WoW root file entries.

## Examples
### Finding names for a specific hash using a pattern
Unknown characters can be specified using either `*` or `%` characters.
```
>bruteforcer -n 8aaf1e907b81e722 -p Interface/Cinematics/Logo_****.avi
Interface/Cinematics/Logo_1024.avi
>bruteforcer -n 8aaf1e907b81e722 -p Interface/Cinematics/Logo_%%%%.avi
Interface/Cinematics/Logo_1024.avi
```

### Using a file to compare against multiple hahses at once
**lookup.csv**
```
5917326;5c3c172fb5afe9a8
5917328;1a7f618193af4317
```
```
>bruteforcer -n lookup.csv -p CREATURE/MAGMASPHERE/MagmaSphere_****.blp
5917326;CREATURE/MAGMASPHERE/MagmaSphere_rock.blp
5917328;CREATURE/MAGMASPHERE/MagmaSphere_drip.blp
```

### Using mirrored wildcards for patterns where a section of text is expected to repeat
When `*` and `%` are used in the same pattern, the first N instances of will match. In this example, the value of `****` is always equal to the value of `%%%%`. If the number of `*`s is not equal to the number of `%`s, then only the leftmost sequences of equal length will match.
```
>bruteforcer -n d3a6007a88ee14fc -p world/maps/****/%%%%.wdt
world/maps/2875/2875.wdt
```

### Changing the alphabet
By default, the alphabet used is 39 characters long `ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- `. Sometimes, you will want to use a shorter alphabet to speed up results. In this example, there are 8 `*`s and the alphabet contains 10 characters, resulting in `10^8` possible names. This is a very small fraction of the `39^8` names that would need to be checked using the default alphabet.
```
>bruteforcer -n lookup.csv -p world/maps/****/%%%%_**_**.adt -a 0123456789
6851560;world/maps/2868/2868_41_25.adt
6851564;world/maps/2868/2868_41_26.adt
6798793;world/maps/2868/2868_43_26.adt
6798801;world/maps/2868/2868_43_27.adt
6798797;world/maps/2868/2868_42_27.adt
6402553;world/maps/2868/2868_42_28.adt
6402549;world/maps/2868/2868_41_28.adt
6402561;world/maps/2868/2868_41_29.adt
6750848;world/maps/2868/2868_40_29.adt
6798789;world/maps/2868/2868_42_26.adt
6851568;world/maps/2868/2868_41_27.adt
6750844;world/maps/2868/2868_40_28.adt
6402557;world/maps/2868/2868_43_28.adt
6402565;world/maps/2868/2868_42_29.adt
```

### Checking multiple patterns from a file
Each line of the file should contain one pattern to evaluate. You can use an optional `;` to specify a different alphabet for each pattern. Any patterns without an alphabet will use the default alphabet given by the `-a` option.
**test_patterns.txt**
```
world/maps/Azeroth/Azeroth.***
world/maps/PvPZone**/PvPZone%%.wdt;0123456789
```
```
>bruteforcer -n lookup.csv -f test_patterns.txt
775970;world/maps/Azeroth/Azeroth.wdl
775971;world/maps/Azeroth/Azeroth.wdt
775969;world/maps/Azeroth/Azeroth.tex
790469;world/maps/PvPZone05/PvPZone05.wdt
790112;world/maps/PvPZone01/PvPZone01.wdt
790291;world/maps/PvPZone03/PvPZone03.wdt
790377;world/maps/PvPZone04/PvPZone04.wdt
861092;world/maps/PvPZone02/PvPZone02.wdt
```

### Use an existing listfile and various strategies to try to match new names
This will take a long time to run compared to the other examples on this page. For my computer with 24 CPU threads it typically takes just over 8 minutes. Currently, this simply checks many combinations of existing directories and filenames contained in the listfile in an attempt to find existing files that were copied to somewhere else without changing the filename. Because a listfile was provided, this will exclude any hashes from the file given with `-n` that are already known.
```
>bruteforcer -n lookup.csv -l listfile-withcapitals.csv
3169803;shaders/raytracing/dx_6_0/TestShader1_Internal.bls
```
