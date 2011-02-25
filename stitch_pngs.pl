#!/usr/bin/perl

use strict;
use Getopt::Std;
use Image::Magick;

#get parameters
my %options;
getopts("o:n:h",\%options);
if (defined $options{h}) {
	print "useage: perl stitch_pngs.pl -o output_file_name -n number_of_pieces\n";
	exit;
}
die "output file name required, use -o 'out_name.png'" unless (defined $options{o});
die "number of PNG pieces required, use -n 'number_of_pieces'" unless (defined $options{n});

#read input images
my @inputs;
my @ys;
my $width = 0;
my $height = 0;
my $piece_height;
my $n;
for ($n = 0; $n < $options{n}; ++$n) {
	my $img = Image::Magick->new;
	my $err = $img->Read("png:" . $options{o} . '.' . $n);
	die "$err" if "$err";
	$width = $img->Get('columns');
	my $h = $img->Get('rows');
	$height += $h;
	if ($n == 0) {
		$piece_height = $h;
	}
	push(@ys, $n * $piece_height);
	push(@inputs, $img);
}

#create output
my $output = Image::Magick->new(size=>$width . 'x' . $height);
$output->Read('xc:transparent');

#composite inputs
for ($n = 0; $n < $options{n}; ++$n) {
	$output->Composite(image=>@inputs[$n], compose=>'over', y=>($height - @ys[$n] - $piece_height));
}

#save output
print "writing: " . $options{o} . "\n";
$output->Write('png:' . $options{o});
