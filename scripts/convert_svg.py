from cairosvg import svg2png
svg_in = r'assets/images/tomato_logo.svg'
png_out = r'assets/images/tomato_icon.png'
svg2png(url=svg_in, write_to=png_out, output_width=1024, output_height=1024)
print('Wrote', png_out)
