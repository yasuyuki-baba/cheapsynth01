"""Print bounded failure diagnostics even when per-sample assertions flood the log."""

import sys
import xml.etree.ElementTree as ET

report = ET.parse(sys.argv[1]).getroot()
print(f"Tests: {report.get('tests')}, failures: {report.get('failures')}")
for case in report.iter("testcase"):
    failures = case.findall("failure")
    if failures:
        print(f"\n{case.get('classname')}.{case.get('name')}: {len(failures)} assertions")
        print((failures[0].get("message") or failures[0].text or "")[:2000])
