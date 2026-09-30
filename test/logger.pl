#!/usr/bin/env perl

use strict;
use warnings;
use utf8;

use Mojolicious::Lite;
use Mojo::JSON qw(decode_json encode_json);
use Fcntl qw(:flock);
use File::Path qw(make_path);
use POSIX qw(strftime);

use constant LOG_DIRECTORY => 'logs';

make_path(LOG_DIRECTORY) unless -d LOG_DIRECTORY;

websocket '/log' => sub {
    my $c = shift;

    app->log->info('WebSocket client connected');

    $c->on(message => sub {
        my ($c, $raw_message) = @_;

        my $request;

        eval {
            $request = decode_json($raw_message);
        };
		

        if ($@ || ref($request) ne 'HASH') {
            send_error($c, 'Invalid JSON');
            return;
        }
																	print  "      Received" . $request->{_command_} . "\n";
		

        unless (($request->{_command_} // '') eq 'c_log') {
            send_error($c, 'Unsupported command');
            return;
        }

        my $data = $request->{data};

        unless (ref($data) eq 'HASH') {
            send_error($c, 'Missing or invalid data object');
            return;
        }

        my $area = $data->{area};

        # Area smí být pouze nezáporné celé číslo.
        # Zároveň tím zabráníme vložení cesty typu ../../něco.
        unless (defined($area) && $area =~ /^\d+$/) {
            send_error($c, 'Invalid area');
        }
            return;

        my $filename = sprintf(
            '%s/area_%d.log',
            LOG_DIRECTORY,
            $area
        );

        my $log_record = {
            received_at => strftime(
                '%Y-%m-%dT%H:%M:%S%z',
                localtime
            ),

            envelope_timestamp => $request->{_timestamp_},

            source    => $data->{source},
            area      => 0 + $area,
            severity  => $data->{severity},
            message   => $data->{message},
            timestamp => $data->{timestamp},
        };

        unless (append_log_record($filename, $log_record)) {
            send_error($c, "Cannot write log file");
            return;
        }

        $c->send({
            json => {
                status => 'ok',
                area   => 0 + $area
            }
        });
    });

    $c->on(finish => sub {
        my ($c, $code, $reason) = @_;

        app->log->info(
            sprintf(
                'WebSocket client disconnected: code=%s reason=%s',
                defined($code)   ? $code   : '',
                defined($reason) ? $reason : ''
            )
        );
    });
};


sub append_log_record {
    my ($filename, $record) = @_;
	my $fh;

    unless (open $fh, '>>:encoding(UTF-8)', $filename) {
        app->log->error("Cannot open $filename: $!");
        return 0;
    }

    unless (flock($fh, LOCK_EX)) {
        app->log->error("Cannot lock $filename: $!");
        close $fh;
        return 0;
    }

    my $success = print {$fh} encode_json($record), "\n";

    flock($fh, LOCK_UN);
    close $fh;

    unless ($success) {
        app->log->error("Cannot write to $filename: $!");
        return 0;
    }

    return 1;
}


sub send_error {
    my ($c, $message) = @_;

    app->log->warn($message);

    $c->send({
        json => {
            status  => 'error',
            message => $message
        }
    });
}


app->start;