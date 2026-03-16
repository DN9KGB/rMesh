#!/usr/bin/env python3
"""
Extracts the section for a specific version tag from CHANGELOG.md.
Prints the matching section to stdout (for use as GitHub release body).
Usage: python extract_changelog.py <tag>
"""
import re
import sys

tag = sys.argv[1] if len(sys.argv) > 1 else None

with open('CHANGELOG.md', encoding='utf-8') as f:
    content = f.read()

# Split into sections at each '## ' heading
sections = re.split(r'\n(?=## )', content.strip())

if tag:
    # Normalize tag for comparison (strip leading 'v' or 'V' for loose matching)
    tag_norm = tag.lstrip('vV').lower()
    for section in sections:
        # Extract version string from heading like ## [v1.0.22] or ## v1.0.22
        m = re.match(r'## \[?([^\]\n]+)\]?', section)
        if m and m.group(1).lstrip('vV').lower() == tag_norm:
            print(section.strip())
            sys.exit(0)
    # Tag not found — fall through to first section
    print(f'(No changelog entry found for {tag})', file=sys.stderr)

# Fallback: first ## section
for section in sections:
    if section.startswith('## '):
        print(section.strip())
        sys.exit(0)

sys.exit(1)
