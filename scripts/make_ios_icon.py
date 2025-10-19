from PIL import Image
src = 'assets/images/tomato_icon.png'
dst = 'assets/images/tomato_icon_ios.png'
img = Image.open(src).convert('RGBA')
bg = Image.new('RGBA', img.size, (255,255,255,255))
bg.paste(img, mask=img.split()[3])
bg.convert('RGB').save(dst, format='PNG')
print('Wrote', dst)
