## DESCRIPTION

ls-fuse mounts output of 'ls -lR', 'ls -lRZ' or 'ls -l' as a pseudo filesystem.

Supported output of ls with options:

* -l (mandatory)
* -R (optional)
* -s (optional)
* -Z option isn't supported yet

## BUILDING

lf-fuse uses autotools. To build project run the following commands:

	./bootstrap.sh
	./configure
	make

## EXAMPLE

After building binary you can try it out:

	ls --color=never -lR > test.txt
	mkdir mnt
	./ls-fuse test.txt mnt

After that mnt/ will contain (I hope) files described in test.txt. The
following command will unmount fs:

	fusermount -u mnt

## For future

* ls-fuse will be adapted to work under other unix-like systems (macos, freebsd)
* extended attributes for SELinux will be supported
* multiple input files support (entry of the files will be combine to a single
  filesystem)
* stdin support (this will allow to combine ls-fuse with other unix tools)
