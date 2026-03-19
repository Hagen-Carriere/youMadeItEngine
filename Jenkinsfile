// Jenkinsfile — Declarative pipeline for youMadeItEngine test automation.
//
// Runs the full pytest suite and publishes JUnit XML results.
// Place this file at the repository root.
//
// Jenkins setup:
//   1. Install the "Pipeline" and "JUnit" plugins
//   2. Create a new Pipeline job pointing to this repo
//   3. Set "Script Path" to "Jenkinsfile"
//
// The pipeline triggers on pushes to main/develop and on PRs.

pipeline {
    agent any

    triggers {
        pollSCM('H/5 * * * *')  // Poll for changes every 5 minutes
    }

    environment {
        PYTHONDONTWRITEBYTECODE = '1'
    }

    stages {
        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Setup Python') {
            steps {
                sh '''
                    python3 --version
                    pip3 install --user pytest
                '''
            }
        }

        stage('Run Tests') {
            steps {
                dir('game_engine') {
                    sh '''
                        python3 -m pytest tests/ -v \
                            --junitxml=reports/test_results.xml \
                            --tb=short
                    '''
                }
            }
        }
    }

    post {
        always {
            // Publish JUnit test results to Jenkins dashboard
            junit 'game_engine/reports/test_results.xml'
        }
        success {
            echo 'All tests passed.'
        }
        failure {
            echo 'Tests failed — check the JUnit report for details.'
        }
    }
}
