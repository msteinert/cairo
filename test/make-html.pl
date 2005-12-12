#!/usr/bin/perl
#
# Copyright © 2005 Mozilla Corporation
#
# Permission to use, copy, modify, distribute, and sell this software
# and its documentation for any purpose is hereby granted without
# fee, provided that the above copyright notice appear in all copies
# and that both that copyright notice and this permission notice
# appear in supporting documentation, and that the name of
# Mozilla Corporation not be used in advertising or publicity pertaining to
# distribution of the software without specific, written prior
# permission. Mozilla Corporation makes no representations about the
# suitability of this software for any purpose.  It is provided "as
# is" without express or implied warranty.
#
# MOZILLA CORPORTAION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
# SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS, IN NO EVENT SHALL MOZILLA CORPORATION BE LIABLE FOR ANY SPECIAL,
# INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
# RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
# IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Author: Vladimir Vukicevic <vladimir@pobox.com>
#

##
## Takes all the *.log files in the current directory and spits out
## html to stdout that can be used to view all the test results at once.
##

my $tests = {};

my $teststats = {};

foreach (<*.log>) {
  (open LOG, "$_") || next;
  while (<LOG>) {
    next unless /^TEST: (.*) TARGET: (.*) FORMAT: (.*) RESULT: (.*)$/;
    $tests->{$1} = {} unless $tests->{$1};
    $tests->{$1}->{$2} = {} unless $tests->{$1}->{$2};
    $tests->{$1}->{$2}->{$3} = $4;

    $teststats->{$2} = {"PASS" => 0, "FAIL" => 0, "XFAIL" => 0, "UNTESTED" => 0}
      unless $teststats->{$2};
    ($teststats->{$2}->{$4})++;
  }
  close LOG;
}

my $targeth = {};
my $formath = {};

foreach my $testname (sort(keys %$tests)) {
  my $v0 = $tests->{$testname};
  foreach my $targetname (sort(keys %$v0)) {
    my $v1 = $v0->{$targetname};

    $targeth->{$targetname} = 1;
    foreach my $formatname (sort(keys %$v1)) {
      $formath->{$formatname} = 1;
    }
  }
}

my @targets = sort(keys %$targeth);
my @formats = sort(keys %$formath);

sub printl {
  print @_, "\n";
}

printl '<html><head>';
printl '<title>Cairo Test Results</title>';
printl '<style type="text/css">';
printl '.PASS { border: 2px solid #009900; background-color: #999999; }';
printl '.FAIL { background-color: #990000; }';
printl '.XFAIL { background-color: #999900; }';
printl '.UNTESTED { background-color: #333333; }';
printl 'img { max-width: 15em; min-width: 3em; }';
printl 'td { vertical-align: top; }';
printl '</style>';
printl '<body>';

printl '<table border="1">';
print '<tr><th>Test</th><th>Ref</th>';
foreach my $target (@targets) {
  print '<th>', $target, '</th>';
}
printl '</tr>';

print '<tr><td></td><td></td>';
foreach my $target (@targets) {
  print '<td>';
  print 'PASS: ', $teststats->{$target}->{"PASS"}, '<br>';
  print 'FAIL: ', $teststats->{$target}->{"FAIL"}, '<br>';
  print 'XFAIL: ', $teststats->{$target}->{"XFAIL"}, '<br>';
  print 'UNTESTED: ', $teststats->{$target}->{"UNTESTED"};
  print '</td>';
}
printl '</tr>';

sub testref {
  my ($test, $format, $rest) = @_;
  my $fmtstr = "";
  if ($format eq "rgb24") {
    $fmtstr = "-rgb24";
  }

  return "$test$fmtstr-ref.png";
}

sub testfiles {
  my ($test, $target, $format, $rest) = @_;
  my $fmtstr = "";
  if ($format eq "rgb24") {
    $fmtstr = "-rgb24";
  } elsif ($format eq "argb32") {
    $fmtstr = "-argb32";
  }

  return ("out" => "$test-$target$fmtstr-out.png",
	  "diff" => "$test-$target$fmtstr-diff.png");
}

foreach my $test (sort(keys %$tests)) {
  foreach my $format (@formats) {
    print '<tr><td>', $test, ' (', $format, ')</td>';

    my $testref = testref($test, $format);
    print "<td><img src=\"$testref\"></td>";

    foreach my $target (@targets) {
      my $tgtdata = $tests->{$test}->{$target};
      if ($tgtdata) {
	my $testres = $tgtdata->{$format};
	if ($testres) {
	  my %testfiles = testfiles($test, $target, $format);
	  print "<td class=\"$testres\">";
	  $stats{$target}{$testres}++;
	  if ($testres eq "PASS") {
	    print "<img src=\"", $testfiles{"out"}, "\"></td>";
	  } elsif ($testres eq "FAIL" || $testres eq "XFAIL") {
	    print "<img src=\"", $testfiles{"out"}, "\"><br><hr size=\"1\">";
	    print "<img src=\"", $testfiles{"diff"}, "\">";
	  }
	  print "</td>";
	} else {
	  print '<td></td>';
	}
      } else {
	print '<td></td>';
      }
    }

    print "</tr>\n";
  }
}

print "</table></body></html>\n";

