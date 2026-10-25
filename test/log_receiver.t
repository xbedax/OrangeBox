use strict;
use warnings;
use Test::More;
use Test::Mojo;
use Mojo::File;
use Mojo::JSON qw(decode_json true);
use Cwd qw(abs_path);
use File::Path qw(make_path);
use FindBin;

my $script = abs_path("$FindBin::Bin/logger.pl");
$0 = abs_path($0);
my $directory = "$FindBin::Bin/../.pio/tests/log-receiver-$$";
make_path($directory);
chdir $directory or die "chdir: $!";
my $t = Test::Mojo->new(Mojo::File->new($script));

sub event {
    my ($id, $params) = @_;
    return {_command_ => 'c_log', _timestamp_ => 1790784000, data => {
        source => 'BOX-007', area => 3, severity => 5, message => $id,
        timestamp => 1790784000, messageparams => $params,
    }};
}
sub send_event {
    my ($event, $status) = @_;
    $t->websocket_ok('/log')->send_ok({json => $event})->message_ok
        ->json_message_is('/status', $status)->finish_ok;
}

send_event(event(164, {p2 => '001234', p1 => 7}), 'ok');
send_event(event(173, {}), 'ok');
send_event(event(65000, {map {("p$_", $_)} 1 .. 12}), 'ok');
send_event(event(65001, {p1 => true}), 'ok');
send_event(event(164, {p2 => 7}), 'error');
send_event(event('old text log', {}), 'error');
send_event(event(164, []), 'error');
send_event(event(65000, {map {("p$_", $_)} 1 .. 13}), 'error');
send_event(event(164, {p1 => {nested => 1}}), 'error');

open my $fh, '<:raw', 'logs/area_3.log' or die "open log: $!";
my @records = map {decode_json($_)} <$fh>;
close $fh;
is scalar(@records), 4, 'only valid events were stored';
is $records[0]{message}, 164, 'numeric ID retained';
is $records[0]{messageparams}{p1}, 7, 'parameter p1 retained by name';
is $records[0]{messageparams}{p2}, '001234', 'string preserves leading zeros';
is $records[2]{message}, 65000, 'unknown ID retained without dictionary lookup';
is $records[2]{messageparams}{p12}, 12, 'twelfth parameter retained';
ok $records[3]{messageparams}{p1}, 'boolean retained';
done_testing;
