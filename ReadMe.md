ExportTable is a faster implementation of Mathematica's
Export[., "Table"] 
for matrices of real numbers, created in response to https://mathematica.stackexchange.com/questions/120015/fast-way-to-export-large-amount-of-data-in-table-format/

by Paul Frischknecht, 2016

License: WTFPL

# Installation
Download as a zip and use the Install function from the menu or put it somewhere where `$Path` can find it,
then load it as

    << ExportTable`

and use it as

    ExportTable["filename.txt", data]
  
# Feedback

Use the Issues feature of github.
