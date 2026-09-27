"""Create an offline browser test from the real UI, replacing only networking."""
from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
html = (root / "UI/index.html").read_text(encoding="utf-8")

def inline_script(match):
    path = match.group(1)
    if path == "js/websoc.js":
        source = """
        const testMessages = [];
        function postFormData(data, command) { testMessages.push({data, command}); }
        let lastContact = Date.now(), connectionState = 2;
        const driftPoolLength = 1, driftPool = [0];
        """
    else:
        source = (root / "UI" / path).read_text(encoding="utf-8")
    return "<script>\n" + source + "\n</script>"

html = re.sub(r'<script src="([^"]+)"[^>]*></script>', inline_script, html)
# Do not fetch stylesheet/font/image assets during a logic test.
html = re.sub(r'<link\b[^>]*>', '', html)
html = re.sub(r'<img\b[^>]*>', '', html)
test_script = (root / "test/cred_ui.js").read_text(encoding="utf-8")
html = html.replace('</body>', '<script>\n' + test_script + '\n</script></body>')
output = root / '.pio/build/cred-ui/test.html'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(html, encoding='utf-8')
print(output)
