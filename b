#!/bin/rc

suffixes='\.([bcChlmsy]|asm|awk|cc|cgi|cpp|cs|go|goc|hs|java|lua|lx|mk|ml|mli|ms|myr|pl|py|rc|sh|tex|xy)$'
fullnames='(^|/)mkfile$'

walk -f |grep -v .git |grep -e $fullnames -e $suffixes |fm
