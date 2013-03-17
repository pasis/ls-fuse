= DESCRIPTION

ls-fuse mounts output of 'ls -lR', 'ls -lRZ' or 'ls -l' as a pseudo filesystem.

This is on early stage of development!

= BUILDING

At the moment you can try it out in the following way:
	cd src && make
	ls --color=never -l > test.txt
	mkdir mnt
	./a.out test.txt mnt

After that mnt/ will contain (I hope) files described in test.txt.

= For future

* ls-fuse will be adapted to work under other *nix systems (macos, freebsd)
* extended attributes for SELinux will be supported
* multiple input files support (entry of the files will be combine to a single
  filesystem)
* stdin support (this will allow to combine ls-fuse with other unix tools)
