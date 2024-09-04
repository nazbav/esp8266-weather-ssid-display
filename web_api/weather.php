<?php
// Конфигурация
date_default_timezone_set('Europe/Moscow');

function fetchWeatherData() {
    $latitude = 48.0000;
    $longitude = 44.0000;
    $url = "https://api.open-meteo.com/v1/forecast?latitude=$latitude&longitude=$longitude&hourly=apparent_temperature,weather_code,wind_speed_10m,relative_humidity_2m&timezone=Europe/Moscow";

    $ch = curl_init();
    curl_setopt($ch, CURLOPT_URL, $url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

    $response = curl_exec($ch);

    if ($response === FALSE) {
        die('Error occurred while fetching weather data: ' . curl_error($ch));
    }

    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    if ($httpCode != 200) {
        die('HTTP Error: ' . $httpCode);
    }

    curl_close($ch);

    $data = json_decode($response, true);

    if (isset($data['error']) && $data['error']) {
        die('API Error: ' . $data['reason']);
    }

    return $data;
}

$data = fetchWeatherData();

if (isset($data['hourly'])) {
    $hourlyWeather = $data['hourly'];

    $currentHour = (int) date('H');

    // Define indices based on the time of day
    $morningStart = 6;
    $dayStart = 12;
    $eveningStart = 18;
    $nightStart = 0;

    $morningEnd = 12;
    $dayEnd = 18;
    $eveningEnd = 24;
    $nightEnd = 6;

    function getClosestIndex($data, $hour) {
        $closestIndex = -1;
        $closestDiff = PHP_INT_MAX;

        foreach ($data['time'] as $index => $time) {
            $timestamp = strtotime($time);
            $hourDiff = abs($timestamp - strtotime("today $hour:00"));
            if ($hourDiff < $closestDiff) {
                $closestDiff = $hourDiff;
                $closestIndex = $index;
            }
        }

        return $closestIndex;
    }

    $currentIndex = getClosestIndex($hourlyWeather, $currentHour);
    $morningIndex = getClosestIndex($hourlyWeather, $morningStart);
    $dayIndex = getClosestIndex($hourlyWeather, $dayStart);
    $eveningIndex = getClosestIndex($hourlyWeather, $eveningStart);
    $nightIndex = getClosestIndex($hourlyWeather, $nightStart);

    function getMinMax($data, $startIndex, $endIndex) {
        if ($startIndex === -1 || $endIndex === -1) {
            return ['temp_min' => 'N/A', 'temp_max' => 'N/A'];
        }

        $slice = array_slice($data['apparent_temperature'], $startIndex, $endIndex - $startIndex + 1);
        return [
            'temp_min' => min($slice),
            'temp_max' => max($slice)
        ];
    }

    $morningEndIndex = getClosestIndex($hourlyWeather, $morningEnd);
    $dayEndIndex = getClosestIndex($hourlyWeather, $dayEnd);
    $eveningEndIndex = getClosestIndex($hourlyWeather, $eveningEnd);
    $nightEndIndex = getClosestIndex($hourlyWeather, $nightEnd);

    $morningData = getMinMax($hourlyWeather, $morningIndex, $morningEndIndex);
    $dayData = getMinMax($hourlyWeather, $dayIndex, $dayEndIndex);
    $eveningData = getMinMax($hourlyWeather, $eveningIndex, $eveningEndIndex);
    $nightData = getMinMax($hourlyWeather, $nightIndex, $nightEndIndex);

    // Определение, прошел ли текущий временной интервал
    $isMorningNextDay = $currentHour >= $morningEnd;
    $isDayNextDay = $currentHour >= $dayEnd;
    $isEveningNextDay = $currentHour >= $eveningEnd;
    $isNightNextDay = $currentHour >= $nightEnd || $currentHour < $morningStart;

    // Добавляем смайлик для следующего дня
    $nextDayEmoji = " 🕒";

    $weatherData = [
        'currentWeather' => [
            'temperature' => $hourlyWeather['apparent_temperature'][$currentIndex] ?? 'N/A',
            'weathercode' => $hourlyWeather['weather_code'][$currentIndex] ?? 'N/A',
            'windspeed' => $hourlyWeather['wind_speed_10m'][$currentIndex] ?? 'N/A',
            'humidity' => $hourlyWeather['relative_humidity_2m'][$currentIndex] ?? 'N/A',
        ],
        'morning' => [
            'temp_min' => $morningData['temp_min'],
            'temp_max' => $morningData['temp_max'],
            'weathercode' => $hourlyWeather['weather_code'][$morningIndex] ?? 'N/A',
            'windspeed' => min(array_slice($hourlyWeather['wind_speed_10m'], $morningIndex, $morningEndIndex - $morningIndex + 1)) ?? 'N/A',
            'humidity' => min(array_slice($hourlyWeather['relative_humidity_2m'], $morningIndex, $morningEndIndex - $morningIndex + 1)) ?? 'N/A',
            'is_next_day' => $isMorningNextDay ? $nextDayEmoji : ''
        ],
        'day' => [
            'temp_min' => $dayData['temp_min'],
            'temp_max' => $dayData['temp_max'],
            'weathercode' => $hourlyWeather['weather_code'][$dayIndex] ?? 'N/A',
            'windspeed' => min(array_slice($hourlyWeather['wind_speed_10m'], $dayIndex, $dayEndIndex - $dayIndex + 1)) ?? 'N/A',
            'humidity' => min(array_slice($hourlyWeather['relative_humidity_2m'], $dayIndex, $dayEndIndex - $dayIndex + 1)) ?? 'N/A',
            'is_next_day' => $isDayNextDay ? $nextDayEmoji : ''
        ],
        'evening' => [
            'temp_min' => $eveningData['temp_min'],
            'temp_max' => $eveningData['temp_max'],
            'weathercode' => $hourlyWeather['weather_code'][$eveningIndex] ?? 'N/A',
            'windspeed' => min(array_slice($hourlyWeather['wind_speed_10m'], $eveningIndex, $eveningEndIndex - $eveningIndex + 1)) ?? 'N/A',
            'humidity' => min(array_slice($hourlyWeather['relative_humidity_2m'], $eveningIndex, $eveningEndIndex - $eveningIndex + 1)) ?? 'N/A',
            'is_next_day' => $isEveningNextDay ? $nextDayEmoji : ''
        ],
        'night' => [
            'temp_min' => $nightData['temp_min'],
            'temp_max' => $nightData['temp_max'],
            'weathercode' => $hourlyWeather['weather_code'][$nightIndex] ?? 'N/A',
            'windspeed' => min(array_slice($hourlyWeather['wind_speed_10m'], $nightIndex, $nightEndIndex - $nightIndex + 1)) ?? 'N/A',
            'humidity' => min(array_slice($hourlyWeather['relative_humidity_2m'], $nightIndex, $nightEndIndex - $nightIndex + 1)) ?? 'N/A',
            'is_next_day' => $isNightNextDay ? $nextDayEmoji : ''
        ]
    ];

    header('Content-Type: application/json');
    echo json_encode($weatherData);
} else {
    die('Invalid data structure received from weather API');
}
?>
