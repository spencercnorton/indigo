from pathlib import Path
import copy
import contextlib
import io
from unittest import mock
import json
import subprocess
import tempfile
import unittest

import check_memcard_qualification as qualification

LAUNCH = ("static void gameflowWaitForCardIds(void);\n\n"
          "static void gameflowWaitForCardIds(void)\n{\n\twhile(waiting()) poll();\n}\n\n"
          "static void load_app(void)\n{\n\tif(emulate) {\n\t}\n\telse {\n"
          "\t\tinitialize_card(CARD_SLOTA);\n\t\tinitialize_card(CARD_SLOTB);\n\t}\n}\n")


class Qualification(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for relative in qualification.SOURCES:
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("demonstration source\n")
        launch = self.root / qualification.LAUNCH
        launch.parent.mkdir(parents=True, exist_ok=True)
        launch.write_text(LAUNCH)
        for name in ["ci.yml","release.yml"]:
            path=self.root/".github/workflows"/name
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text("env:\n  TOOLCHAIN: demonstration-toolchain\n")
        subprocess.run(["git","init","-q",str(self.root)],check=True,capture_output=True)
        subprocess.run(["git","-C",str(self.root),"add","."],check=True,capture_output=True)
        subprocess.run(["git","-C",str(self.root),"-c","commit.gpgsign=false",
                        "-c","user.name=Test","-c","user.email=test@example.invalid",
                        "commit","-qm","Demonstration source"],check=True,capture_output=True)
        revision=subprocess.run(["git","-C",str(self.root),"rev-parse","HEAD"],
                                check=True,text=True,capture_output=True).stdout.strip()
        self.record = self.root / qualification.RECORD
        self.record.parent.mkdir(parents=True)
        self.good = {
            "status":"passed", "environment":"real-hardware",
            "source_fingerprint":qualification.fingerprint(self.root),
            "tested_revision":revision,
            "hardware":{"device":"MemCard PRO GC","firmware":"test-version",
                        "setup":"demonstration console and loader"},
            "evidence":"https://example.invalid/hardware-test",
            "results":dict.fromkeys(qualification.CHECKS,True),
        }

    def write(self, record):
        self.record.write_text(json.dumps(record))

    def test_pending_record_holds_stable_but_allows_test_channels(self):
        self.write({"status":"pending"})
        self.assertTrue(qualification.problems(self.root,"v2.4.0"))
        for tag in ["v2.4.0-beta.1","v2.4.0-rc.1"]:
            self.assertEqual(qualification.problems(self.root,tag),[])

    def test_informational_reports_expose_pending_coverage_without_changing_it(self):
        self.write({"status": "pending"})
        before = self.record.read_bytes()
        for tag in ["v2.5.0", "v2.5.0-rc.3"]:
            with self.subTest(tag=tag), mock.patch.object(qualification, "ROOT", self.root), \
                    mock.patch("sys.argv", ["qualification", tag, "--report"]), \
                    contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(qualification.main(), 0)
                self.assertIn("pending real-card qualification", output.getvalue())
                self.assertNotIn("recorded real-card pass matches", output.getvalue())
        self.assertEqual(self.record.read_bytes(), before)

    def test_strict_audit_still_rejects_a_pending_hardware_claim(self):
        self.write({"status": "pending"})
        with mock.patch.object(qualification, "ROOT", self.root), \
                mock.patch("sys.argv", ["qualification", "v2.5.0"]), \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(qualification.main(), 1)

    def test_real_complete_record_and_matching_code_allow_stable(self):
        self.write(self.good)
        self.assertEqual(qualification.problems(self.root,"v2.4.0"),[])

    def test_lab_evidence_missing_checks_and_missing_identity_do_not_qualify(self):
        bad = []
        for key,value in [("environment","emulator"),("status","pending"),
                          ("tested_revision",None),("evidence",None),("hardware",{})]:
            record=copy.deepcopy(self.good); record[key]=value; bad.append(record)
        for check in qualification.CHECKS:
            record=copy.deepcopy(self.good); record["results"][check]=False; bad.append(record)
        for record in bad:
            with self.subTest(record=record):
                self.write(record)
                self.assertTrue(qualification.problems(self.root,"v2.4.0"))

    def test_changed_switching_code_requires_requalification(self):
        self.write(self.good)
        (self.root / qualification.SOURCES[0]).write_text("changed switching code\n")
        self.assertTrue(qualification.problems(self.root,"v2.4.0"))
        changed=copy.deepcopy(self.good)
        changed["source_fingerprint"]=qualification.fingerprint(self.root)
        self.write(changed)
        self.assertTrue(qualification.problems(self.root,"v2.4.0"),
                        "updating the fingerprint cannot qualify an older tested build")

    def test_changed_launch_around_a_switch_requires_requalification(self):
        self.write(self.good)
        launch = self.root / qualification.LAUNCH
        for old,new in [("\t\tinitialize_card(CARD_SLOTA);\n",
                         "\t\tif(readable(0))\n\t\t\tinitialize_card(CARD_SLOTA);\n"),
                        ("while(waiting())","while(waiting() || switching())")]:
            with self.subTest(old=old):
                launch.write_text(LAUNCH.replace(old,new,1))
                self.assertTrue(qualification.problems(self.root,"v2.4.0"))

    def test_the_rest_of_swiss_c_is_the_releases_own(self):
        self.write(self.good)
        (self.root/qualification.LAUNCH).write_text("static int detail;\n" + LAUNCH)
        self.assertEqual(qualification.problems(self.root,"v2.4.0"),[])

    def test_missing_malformed_and_nonobject_records_fail_closed(self):
        self.assertTrue(qualification.problems(self.root,"v2.4.0"))
        for value in ["broken json", "[]", "null"]:
            self.record.write_text(value)
            self.assertTrue(qualification.problems(self.root,"v2.4.0"))

    def test_changed_library_image_requires_requalification(self):
        self.write(self.good)
        (self.root/".github/workflows/ci.yml").write_text("env:\n  TOOLCHAIN: changed-toolchain\n")
        self.assertTrue(qualification.problems(self.root,"v2.4.0"))

    def test_a_release_without_the_switching_feature_needs_no_record(self):
        (self.root / qualification.SOURCES[0]).unlink()
        self.assertEqual(qualification.problems(self.root,"v2.3.0"),[])


if __name__ == "__main__":
    unittest.main()
