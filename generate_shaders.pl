#!/usr/bin/env perl
# Shader code generator for Milton
# 
# This Perl script replaces the previous C++ shadergen tool. It reads GLSL 
# shader files and generates a C++ header (shaders.gen.h) with embedded 
# shader source code as string literals.
#
# Benefits over the C++ version:
# - No separate compilation step needed
# - Perl is already a build dependency
# - Easier to maintain and modify
# - Standard, mainstream approach
#
# Usage: ./generate_shaders.pl <source_dir> <output_file>

use strict;
use warnings;
use FindBin;
use File::Basename;

my $source_dir = $ARGV[0] || die "Usage: $0 <source_dir> <output_file>\n";
my $output_file = $ARGV[1] || die "Usage: $0 <source_dir> <output_file>\n";

# Shader files to process (path relative to source_dir, optional prelude file)
my @shaders = (
    ['picker.v.glsl'],
    ['picker.f.glsl'],
    ['layer_blend.v.glsl'],
    ['layer_blend.f.glsl'],
    ['simple.v.glsl'],
    ['simple.f.glsl'],
    ['outline.v.glsl'],
    ['outline.f.glsl'],
    ['stroke_raster.v.glsl', 'common.glsl'],
    ['stroke_raster.f.glsl', 'common.glsl'],
    ['stroke_eraser.f.glsl', 'common.glsl'],
    ['stroke_info.f.glsl', 'common.glsl'],
    ['stroke_fill.f.glsl', 'common.glsl'],
    ['stroke_clear.f.glsl', 'common.glsl'],
    ['stroke_debug.f.glsl', 'common.glsl'],
    ['exporter_rect.f.glsl'],
    ['texture_fill.f.glsl'],
    ['quad.v.glsl'],
    ['quad.f.glsl'],
    ['postproc.f.glsl', '../third_party/Fxaa3_11.f.glsl'],
    ['blur.f.glsl'],
);

sub read_shader_file {
    my ($filepath) = @_;
    open(my $fh, '<', $filepath) or die "Cannot open shader file '$filepath': $!\n";
    my @lines = <$fh>;
    close($fh);
    return @lines;
}

sub shader_to_varname {
    my ($filename) = @_;
    my $basename = basename($filename);
    # Convert "shader.v.glsl" to "g_shader_v"
    $basename =~ s/\.glsl$//;
    $basename =~ s/\./_/;
    return "g_$basename";
}

sub escape_line {
    my ($line) = @_;
    chomp($line);
    $line =~ s/\r//g;  # Remove carriage returns
    $line =~ s/"/Q/g;  # Replace quotes with 'Q' (shadergen compatibility)
    return $line;
}

# Generate the header file
open(my $out, '>', $output_file) or die "Cannot write to '$output_file': $!\n";

print STDERR "Generating shader code to $output_file...\n";

foreach my $shader_info (@shaders) {
    my ($shader_file, $prelude_file) = @$shader_info;
    my $shader_path = "$source_dir/$shader_file";
    
    my $varname = shader_to_varname($shader_file);
    
    print $out "static char ${varname}[] = \n";
    
    # If there's a prelude file, include it first
    if ($prelude_file) {
        my $prelude_path = "$source_dir/$prelude_file";
        if (-f $prelude_path) {
            my @prelude_lines = read_shader_file($prelude_path);
            foreach my $line (@prelude_lines) {
                my $escaped = escape_line($line);
                print $out "\"${escaped}\\n\"\n";
            }
        }
    }
    
    # Read and output the main shader
    if (-f $shader_path) {
        my @lines = read_shader_file($shader_path);
        foreach my $line (@lines) {
            my $escaped = escape_line($line);
            print $out "\"${escaped}\\n\"\n";
        }
    } else {
        warn "Warning: Shader file '$shader_path' not found\n";
    }
    
    print $out ";\n";
}

close($out);

print STDERR "Shaders generated OK\n";
exit 0;
