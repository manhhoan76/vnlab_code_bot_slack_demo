<?php

namespace app\controllers;

use app\models\BotInfo;
use app\models\remindChannel;
use app\models\RemindProcess;

class BotCheckController extends BaseController
{
    const TOKEN = "xoxb-2523231391122-2832477402738-gd8BfSiJRBfIVrYcUvWtHijk";
    const URL_POST_MESSAGE = "https://slack.com/api/chat.postMessage"; //post
    const URL_GET_MESSAGE_HISTORY = "https://slack.com/api/conversations.history"; //get
    const URL_GET_CHANNEL_MEMBERS = "https://slack.com/api/conversations.members";
    const LINK_SLACK_APP = "https://vnlabcenter.slack.com/archives/";
    const FLG_SLACK_NOTIFY = 1;
    const FLG_CONFIG_FALSE = 0;
    const FLG_CONFIG_TRUE = 2;


    /**
     * index.
     *
     * @return action
     */
    public function actionIndex()
    {
        date_default_timezone_set('Asia/Saigon');
        $remind_process = new RemindProcess();
        $bot = new BotInfo();
        $send_list = $bot->getListBotSend();
        foreach ($send_list as $bot) {
            $url = $this::URL_POST_MESSAGE;
            $data = [
                "token" => $this::TOKEN,
                "channel" => $bot['group_id'], //"#myChannel",
                "text" => $bot['content'],
            ];
            $ch = curl_init();
            curl_setopt($ch, CURLOPT_URL, $url);
            curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
            curl_setopt($ch, CURLOPT_POSTFIELDS, $data);
            $response = curl_exec($ch);
            curl_close($ch);
            $array_data = [];
            $array_data = json_decode($response, 16);
            if ($bot['remind']) {
                if (!$remind_process->getListRemindProcess($array_data['ts'], $bot['group_id'])) {
                    //Add new process
                    $remind_process->addRemindProcessBot($array_data['ts'], $bot['group_id'], time() + $bot['time_remind'], $bot['text_remind'], 1);
                }
            } else {
                if (!$remind_process->getListRemindProcess($array_data['ts'], $bot['group_id'])) {
                    //Add new process no send
                    $remind_process->addRemindProcessNoSend($array_data['ts'], $bot['group_id'], time(), NULL, 1);
                }
            }
        }
        $this->getMessage();
        $this->checkProcessSendMessages();
        return;
    }

    /**
     * .
     *
     * @return 
     */
    public function getMessage()
    {
        $remind_channel = new RemindChannel();
        $remind_process = new RemindProcess();
        $list_channel = $remind_channel->getListRemindChannel();
        //check for channel
        foreach ($list_channel as $Channel_details) {
            $max_process = $remind_process->getListRemindprocess_Maxts($Channel_details['id_channel']);
            $data_post = [];
            // If channels data already exist  
            if ($max_process) {
                $data_post = [
                    "token" => $this::TOKEN,
                    "channel" => $Channel_details['id_channel'], //"#myChannel",
                    'oldest' => $max_process,
                ];
            }
            // First get data channel
            else {
                $data_post = [
                    "token" => $this::TOKEN,
                    "channel" => $Channel_details['id_channel'], //"#myChannel",
                    'limit' => 1,
                    'ts' => 'latest',
                ];
            }
            $array_message_data = $this->getSlackHistory($data_post);
            //check response
            if (!$array_message_data['ok']) {
                //next chanel
                continue;
            }
            // Set process send message
            foreach ($array_message_data["messages"] as $message) {
                //check process exits 
                if (!$remind_process->getListRemindProcess($message['ts'], $Channel_details['id_channel'])) {
                    //Add new process
                    $remind_process->AddRemindProcess($message['ts'], $Channel_details['id_channel'], time() + $Channel_details['time_remind'], NULL, 0);
                }
            }
        }
    }

    private function checkProcessSendMessages()
    {
        $remind_process = new RemindProcess();
        $remind_channel = new RemindChannel();
        //check process remind
        $list_process = $remind_process->getListRemindProcess_remind();
        // scan list remind
        foreach ($list_process as $process) {
            $check_see = [];
            $flg_notify_channel = $this::FLG_CONFIG_FALSE;
            $flg_notify_user = $this::FLG_CONFIG_FALSE;
            $list_notify = [];
            $member = $this->getAllMember($process['id_channel'])['members'];
            foreach ($member as $mem) {
                $check_see[$mem] = false;
            }
            $message = $this->getSlackMessages($process['id_channel'], $process['ts']);
            // Check response
            if ($message['ok']) {
                if (isset($message['messages'][0]['reactions'])) {
                    foreach ($message['messages'][0]['reactions'] as $reactions) {
                        foreach ($reactions['users'] as $id_user) {
                            //if reactions 
                            $check_see[$id_user] = true;
                        }
                    }
                }
                if (isset($message['messages'][0]['reply_users'])) {
                    foreach ($message['messages'][0]['reply_users'] as $id_user) {
                        //if reply
                        $check_see[$id_user] = true;
                    }
                }
                if (isset($message['messages'][0]['blocks'][0]['elements'])) {
                    foreach ($message['messages'][0]['blocks'][0]['elements'][0]['elements'] as $element) {
                        //if notify channel
                        if ($element['type'] == 'broadcast') {
                            $flg_notify_channel = $this::FLG_SLACK_NOTIFY;
                        }
                        //if notify user 
                        if ($element['type'] == 'user') {
                            $flg_notify_user = $this::FLG_SLACK_NOTIFY;
                            $list_notify[] = $element['user_id'];
                        }
                    }
                }
            } else {
                //next process
                continue;
            }
            //Send messages
            if ($process['is_bot']) {
                //check all user
                foreach ($check_see as $id => $user) {
                    if (!$user) {
                        // if not reactions and reply
                        //send remind
                        $data_post = [
                            "token" => $this::TOKEN,
                            "channel" => $id, //"#id",
                            "text" => " " . $process['text_remind'] . $this::LINK_SLACK_APP . $process['id_channel'] . '/p' . str_replace(".", "", $process['ts']),
                            "as_user" => true,
                        ];
                        $this->postMessage($data_post);
                    }
                }
            } else {
                $list_channel = $remind_channel->getRemindChannelByChannelId($process['id_channel']);
                var_dump($list_channel);die;
                if (!$list_channel) {
                    $remind_process->updateListRemindProcess($process['id']);
                    continue;
                }
                //check config notify channel
                if ($list_channel['check_notify_channel']) {
                    $flg_notify_channel++;
                }
                //check config notify user
                if ($list_channel['send_private']) {
                    $flg_notify_user++;
                }
                var_dump($flg_notify_channel);die;
                //Check notify group or all
                if ((!$list_channel['check_notify_channel'] && !$list_channel['check_notify_user']) || $flg_notify_channel == 2) {
                    //Send all
                    if ($list_channel['send_group'] == 1) {
                        $content = ' ';
                        $content .= $list_channel['text_remind_group'];
                        foreach ($check_see as $id => $check_watched) {
                            // not see
                            if (!$check_watched) {
                                $content .= ' <@' . $id . '> ';
                            }
                        }
                        //send remind to group
                        $data_post = [
                            "token" => $this::TOKEN,
                            "channel" => $process['id_channel'], //"#idchannel",
                            "text" => $content . " " . $this::LINK_SLACK_APP . $process['id_channel'] . '/p' . str_replace(".", "", $process['ts']),
                        ];
                        $this->postMessage($data_post);
                    }
                    if ($list_channel['send_private']) {
                        foreach ($check_see as $id => $check_watched) {
                            // not see
                            if (!$check_watched) {
                                //send to user
                                $data_post = [
                                    "token" => $this::TOKEN,
                                    "channel" => $id, //"#id",
                                    "text" =>  $list_channel['text_remind_private'] . " " . $this::LINK_SLACK_APP . $process['id_channel'] . '/p' . str_replace(".", "", $process['ts']),
                                    "as_user" => true,
                                ];
                                $this->postMessage($data_post);
                            }
                        }
                    }
                } else {
                    //notify user
                    if ($flg_notify_user == $this::FLG_CONFIG_TRUE) {
                        //check send group
                        if ($list_channel['send_group']) {
                            $content = ' ';
                            $content .= $list_channel['text_remind_group'];
                            foreach ($list_notify as $user) {
                                if (!$check_see[$user]) {
                                    $content .= ' <@' . $user . '> ';
                                }
                            }
                            //send remind to group
                            $data_post = [
                                "token" => $this::TOKEN,
                                "channel" => $process['id_channel'], //"#idchannel",
                                "text" => $content . " " . $this::LINK_SLACK_APP . $process['id_channel'] . '/p' . str_replace(".", "", $process['ts']),
                            ];
                            $this->postMessage($data_post);
                        }
                        //send private
                        if ($list_channel['send_private']) {
                            foreach ($list_notify as $user) {
                                if (!$check_see[$user]) {
                                    //send to user
                                    $data_post = [
                                        "token" => $this::TOKEN,
                                        "channel" => $user, //"#id",
                                        "text" =>  $list_channel['text_remind_private'] . " " . $this::LINK_SLACK_APP . $process['id_channel'] . '/p' . str_replace(".", "", $process['ts']),
                                        "as_user" => true,
                                    ];
                                    $this->postMessage($data_post);
                                }
                            }
                        }
                    }
                }
            }
            // flagged message is checked.
            $remind_process->updateListRemindProcess($process['id']);
        }
    }

    private function getSlackMessages($id_channel, $ts)
    {
        $url = $this::URL_GET_MESSAGE_HISTORY;
        $data = [
            "token" => $this::TOKEN,
            "channel" => $id_channel, //"#myChannel",
            "latest" => $ts,
            "inclusive" => "true",
            "limit" => 1,

        ];
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
        curl_setopt($ch, CURLOPT_POSTFIELDS, $data);
        $response = curl_exec($ch);
        curl_close($ch);
        $array_data = [];
        $array_data = json_decode($response, 16);
        return $array_data;
    }

    public function getAllMember($id_Channel)
    {
        $url = $this::URL_GET_CHANNEL_MEMBERS;
        $data = [
            "token" => $this::TOKEN,
            "channel" => $id_Channel, //"#myChannel",
        ];
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
        curl_setopt($ch, CURLOPT_POSTFIELDS, $data);
        $response = curl_exec($ch);
        curl_close($ch);
        $array_data = [];
        $array_data = json_decode($response, 16);
        return $array_data;
    }

    private function  getSlackHistory($data_post)
    {
        $url = $this::URL_GET_MESSAGE_HISTORY;
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
        curl_setopt($ch, CURLOPT_POSTFIELDS, $data_post);
        $response = curl_exec($ch);
        curl_close($ch);
        $array_message = [];
        $array_message = json_decode($response, 16);
        return $array_message;
    }

    private function postMessage($data_post)
    {
        $url = $this::URL_POST_MESSAGE;
        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, $url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, TRUE);
        curl_setopt($ch, CURLOPT_POSTFIELDS, $data_post);
        $response = curl_exec($ch);
        curl_close($ch);
    }
}
