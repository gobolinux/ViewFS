#!/View/ViewFS/0.2.3/bin/python

import os
import re

strip_package_version = re.compile("\./[^/]*/[^/]+/")

for (directory,files,links) in os.walk("."):
   match = strip_package_version.match(directory)
   if not match:
      continue
   path = directory[match.end() - 1:]
   package = directory[2:match.end() - 1]
   print path + ":"
   for file in files + links:
      print path + "/" + file + ":/Packages/" + package + path + "/" + file
