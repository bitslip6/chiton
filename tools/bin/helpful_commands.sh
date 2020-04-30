
alias chiclean='bitbake -c cleanall u-boot-variscite linux-variscite kernel-module-imx-gpu-viv cryptodev-module fsl-image-qt5;bitbake -c cleansstate virtual/kernel'

alias chibake='bitbake fsl-image-qt5'

alias chikernel='bitbake -c clean virtual/kernel; bitbake -c cleansstate virtual/kernel; bitbake virtual/kernel;'

function chiburn() {
	echo "chiburn $1"
	sudo umount $1
	sudo umount $1
	sudo mount $1 /mnt
	read -p "Confirm write to $1 y/n?" -n 1 -r
	echo 
	if [[ $REPLY =~ ^[Yy]$ ]]
	then
		echo "extracting image to $1"
		sudo tar --overwrite --totals -C /mnt -zxf tmp/deploy/images/imx8mm-var-dart/fsl-image-qt5-imx8mm-var-dart.tar.gz
		sudo sync
	fi
}

