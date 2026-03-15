from PIL import Image, ImageDraw, ImageFont

# Load the original image
img_path = '/home/eric/.gemini/antigravity/brain/0af1de37-af29-44ac-8da9-c69d5faaf668/timeline_phase_1773509147658.png'
img = Image.open(img_path)
draw = ImageDraw.Draw(img)

# Try to load a font, fallback to default if not found
try:
    font = ImageFont.truetype("FreeSans.ttf", 20)
    font_bold = ImageFont.truetype("FreeSansBold.ttf", 22)
except IOError:
    font = ImageFont.load_default()
    font_bold = ImageFont.load_default()

# Define the bounding boxes for the text areas to clear (x_min, y_min, x_max, y_max)
# Coordinates estimated from the first generated image (approximate 1024x1024)
# Stage 1: Blue box text
box1_clear = (70, 460, 260, 680)
# Stage 2: Green box text
box2_clear = (300, 460, 490, 680)
# Stage 3: Orange box text
box3_clear = (530, 460, 720, 680)
# Stage 4: Red box text
box4_clear = (760, 460, 950, 680)

# Colors for filling background
colors = {
    1: '#eaf4fc', # Light blue
    2: '#eaf5ee', # Light green
    3: '#fcf4ea', # Light orange
    4: '#fceaea'  # Light red
}

# Clear the old text
draw.rectangle(box1_clear, fill=colors[1])
draw.rectangle(box2_clear, fill=colors[2])
draw.rectangle(box3_clear, fill=colors[3])
draw.rectangle(box4_clear, fill=colors[4])

# Text color
text_color = '#1a1a1a'

# New Text to write
text1 = "- Identify ADV\n  trends\n\n- Analyze key\n  technologies\n\n- Establish\n  research basis"
text2 = "- Evaluate\n  CARLA-StarPU\n  integration\n\n- Validate\n  experimental\n  viability\n\n- Define the\n  methodological\n  basis"
text3 = "- Consolidate\n  Cart-OnePiece\n\n- Support workload\n  investigations\n\n- Enable systematic\n  experimentation"
text4 = "- Execute\n  controlled\n  experiments\n\n- Collect runtime\n  profiling data\n\n- Compare\n  scheduling\n  strategies"

# Draw the new text
# Coordinates adjusted to fit inside the boxes
y_start = 470
draw.text((75, y_start), text1, font=font, fill=text_color, spacing=6)
draw.text((310, y_start), text2, font=font, fill=text_color, spacing=6)
draw.text((545, y_start), text3, font=font, fill=text_color, spacing=6)
draw.text((775, y_start), text4, font=font, fill=text_color, spacing=6)

# Save the updated image
out_path = '/home/eric/Cart_OnePiece/Cart-OnePiece_Antigravity_C++/Tese_Ericson/texto/timeline_phase.png'
img.save(out_path)
img.save('/home/eric/.gemini/antigravity/brain/0af1de37-af29-44ac-8da9-c69d5faaf668/timeline_phase_updated.png')

print(f"Image successfully updated and saved to {out_path}.")
