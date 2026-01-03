#!/usr/bin/env perl
# Shader code generator for Milton
#
# This Perl script assembles SPIR-V from .spvasm and embeds bytecode in a C++
# header (shaders.gen.h).
#
# Usage: ./generate_shaders.pl <source_dir> <output_file>

use strict;
use warnings;
use FindBin;
use File::Basename;
my $source_dir = $ARGV[0] || die "Usage: $0 <source_dir> <output_file>\n";
my $output_file = $ARGV[1] || die "Usage: $0 <source_dir> <output_file>\n";
my $spirv_as = $ENV{SPIRV_AS} || "spirv-as";

# SPIR-V assembly files to process (path relative to source_dir)
my @shaders = (
    ['picker.v.spvasm'],
    ['picker.f.spvasm'],
    ['layer_blend.v.spvasm'],
    ['layer_blend.f.spvasm'],
    ['simple.v.spvasm'],
    ['simple.f.spvasm'],
    ['outline.v.spvasm'],
    ['outline.f.spvasm'],
    ['stroke_raster.v.spvasm'],
    ['stroke_raster.f.spvasm'],
    ['stroke_eraser.f.spvasm'],
    ['stroke_info.f.spvasm'],
    ['stroke_fill.f.spvasm'],
    ['stroke_clear.f.spvasm'],
    ['stroke_debug.f.spvasm'],
    ['exporter_rect.f.spvasm'],
    ['texture_fill.v.spvasm'],
    ['texture_fill.f.spvasm'],
    ['quad.v.spvasm'],
    ['quad.f.spvasm'],
    ['postproc.f.spvasm'],
    ['blur.f.spvasm'],
);

sub shader_to_varname {
    my ($filename) = @_;
    my $basename = basename($filename);
    # Convert "shader.v.spvasm" to "g_shader_v"
    $basename =~ s/\.spvasm$//;
    $basename =~ s/\./_/;
    return "g_$basename";
}

sub read_spirv_words {
    my ($filepath) = @_;
    open(my $fh, '<:raw', $filepath) or die "Cannot open SPIR-V file '$filepath': $!\n";
    my $data;
    read($fh, $data, -s $fh);
    close($fh);
    my $len = length($data);
    die "SPIR-V bytecode length not multiple of 4 for '$filepath'\n" if ($len % 4) != 0;
    my @words = unpack("V*", $data);
    return @words;
}

# Ensure assembler exists before touching output.
if (system("$spirv_as --version > /dev/null 2>&1") != 0) {
    die "spirv-as not found. Set SPIRV_AS or install spirv-tools.\n";
}

# Generate the header file (write to temp, rename on success)
my $tmp_output = "$output_file.tmp";
open(my $out, '>', $tmp_output) or die "Cannot write to '$tmp_output': $!\n";

print STDERR "Generating shader code to $output_file...\n";

print $out "#pragma once\n";
print $out "#include <cstddef>\n\n";

foreach my $shader_info (@shaders) {
    my ($shader_file) = @$shader_info;
    my $shader_path = "$source_dir/$shader_file";
    
    my $varname = shader_to_varname($shader_file);

    if (-f $shader_path) {
        my $spv_path = "$output_file.$shader_file.spv";
        my $cmd = "$spirv_as \"$shader_path\" -o \"$spv_path\"";
        my $ret = system($cmd);
        if ($ret != 0) {
            die "SPIR-V assembly failed for '$shader_file'\n";
        }

        my @words = read_spirv_words($spv_path);
        unlink $spv_path;

        my $spv_var = "${varname}_spv";
        print $out "static const unsigned int ${spv_var}[] = {\n";
        my $col = 0;
        foreach my $w (@words) {
            printf $out "0x%08x,", $w;
            $col++;
            if ($col >= 8) {
                print $out "\n";
                $col = 0;
            } else {
                print $out " ";
            }
        }
        print $out "\n};\n";
        print $out "static const size_t ${spv_var}_size = sizeof(${spv_var});\n\n";
    } else {
        warn "Warning: SPIR-V file '$shader_path' not found\n";
        next;
    }
}

close($out);
rename($tmp_output, $output_file) or die "Cannot replace '$output_file': $!\n";

print STDERR "Shaders generated OK\n";
exit 0;
