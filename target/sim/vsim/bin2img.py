import sys
from PIL import Image
imgdims = (640, 480)
image = Image.new("RGB", (imgdims[0], imgdims[1]), 0);
pixels = image.load()
print("Opening file " + sys.argv[1])
content = 0
with open(sys.argv[1], "rb") as file:
    content = file.read()

print(type(content))
x = 0
y = 0

for y in range(0, imgdims[1]):
    for x in range(0, imgdims[0]):
        idx = 4 * (y * imgdims[0] + x)
        pixels[x, y] = (content[idx], content[idx + 1], content[idx + 2])
image.save(sys.argv[1] + ".png")