#!/bin/sh
while IFS= read -r line || [ -n "$line" ]; do
  rm -f -- "${DESTDIR-}$line" || exit 1
done <install_manifest.txt || exit 1

grep "/include/" install_manifest.txt | awk '{print length, $0}' | sort -rn | uniq | cut -d" " -f2- | while IFS= read -r line; do
  installed_dir=$(dirname -- "${DESTDIR-}$line")
  case "$installed_dir" in
  */include) ;;
  *) rmdir -- "$installed_dir" 2>/dev/null || : ;;
  esac
done

rm -f install_manifest.txt
