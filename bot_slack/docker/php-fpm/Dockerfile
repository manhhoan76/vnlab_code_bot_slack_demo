FROM php:8.0.3-fpm-alpine3.13
# Arguments defined in docker-compose.yml
ARG user
ARG uid

# Install system dependencies
RUN apk update && apk --no-cache add git \
    nodejs \
    npm \
    curl \
    zip \
    unzip \
    zlib-dev \
    libpng-dev \
    icu-dev \
    apk-cron \
    initscripts \
    tini

# Clear cache
RUN rm -rf /var/cache/apk/*

# Install PHP extensions
RUN docker-php-ext-install pdo_mysql exif gd intl tokenizer bcmath pdo

# Get latest Composer
COPY --from=composer:latest /usr/bin/composer /usr/bin/composer

# Create system user to run Composer and Artisan Commands
RUN addgroup -S docker && adduser -S $user -G docker --uid $uid --home /home/$user 
RUN mkdir -p /home/$user/.composer && \
    chown -R $user:docker /home/$user

# RUN mkdir /etc/periodic/midnight
# ADD ./scripts/compute_leaderboards /etc/periodic/midnight/compute_leaderboards
# RUN chmod +x /etc/periodic/midnight/compute_leaderboards
RUN touch crontab.tmp \
    && echo '* * * * * php /var/www/botlabvn/yii bot' > crontab.tmp \
    && crontab crontab.tmp \
    && rm -rf crontab.tmp

CMD ["/usr/sbin/crond", "-f", "-d", "0"]
# Set working directory
WORKDIR /var/www/html

USER $user